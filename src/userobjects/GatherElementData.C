//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "GatherElementData.h"
#include "MooseError.h"
#include <algorithm>

#include "libmesh/mesh_tools.h"
#include "libmesh/nanoflann.hpp"
#include "libmesh/parallel_algebra.h"

registerMooseObject("OptimizationApp", GatherElementData);

// specialization for PointListAdaptor<RadialAverageTop88::QPData>
template <>
const Point &
PointListAdaptor<GatherElementData::ElementData>::getPoint(
    const GatherElementData::ElementData & item) const
{
  return item.center;
}

InputParameters
GatherElementData::validParams()
{
  InputParameters params = ElementUserObject::validParams();
  params.addClassDescription("Computes the filtered sensitivities for "
                             "sensitivity filtering, density filtering or Heaviside projection.");
  params.addRequiredCoupledVar("sensitivities", "Name of the sensitivity variables.");
  params.addParam<VariableName>("design_density", "Design density variable name.");
  params.addParam<VariableName>("filtered_density", "Filtered density variable name.");
  params.set<bool>("force_postaux") = true;
  params.set<int>("execution_order_group") = 0;
  return params;
}

GatherElementData::GatherElementData(const InputParameters & parameters)
  : ElementUserObject(parameters), _n_vars(coupledComponents("sensitivities"))
{
  for (unsigned int i = 0; i < _n_vars; i++)
    _sensitivities.push_back(&writableVariable("sensitivities", i));

  if (_n_vars > 1)
    mooseError("Classic sensitivity filter only requires the objective function sensitivity to "
               "be supplied");
  _design_density_name = getParam<VariableName>("design_density");
  _design_density = &_subproblem.getStandardVariable(_tid, _design_density_name);
}

void
GatherElementData::initialize()
{
  TIME_SECTION("initialize", 2, "Initialize SensitvityFilterCustom");

  gatherElementData();
  // threadJoin(*this);
}

void
GatherElementData::threadJoin(const UserObject & y)
{
  TIME_SECTION("threadJoin", 3, "Joining Threads");
  const GatherElementData & uo = static_cast<const GatherElementData &>(y);
  _elem_data_map.insert(uo._elem_data_map.begin(), uo._elem_data_map.end());
}

void
GatherElementData::finalize()
{
  // the first chunk of data is always the local data - remember its size
  unsigned int local_size = _elem_data_vector.size();

  // communicate the qp data list if n_proc > 1
  if (_app.n_processors() > 1)
  {
    // !!!!!!!!!!!
    // !!CAREFUL!! Is it guaranteed that _elem_data_vector is in the same order if the mesh has not
    // changed? According to @friedmud it is not guaranteed if threads are used
    // !!!!!!!!!!!

    // update after mesh changes and/or if a displaced problem exists
    if (_update_communication_lists || _fe_problem.getDisplacedProblem() ||
        libMesh::n_threads() > 1)
      updateCommunicationLists();

    // data structures for sparse point to point communication
    std::vector<std::vector<ElementData>> send(_candidate_procs.size());
    std::vector<Parallel::Request> send_requests(_candidate_procs.size());
    Parallel::MessageTag send_tag = _communicator.get_unique_tag(4711);
    std::vector<ElementData> receive;

    const auto item_type = TIMPI::StandardType<ElementData>(&(_elem_data_vector[0]));

    // fill buffer and send structures
    for (const auto i : index_range(_candidate_procs))
    {
      const auto pid = _candidate_procs[i];
      const auto & list = _communication_lists[pid];

      // fill send buffer for transfer to pid
      send[i].reserve(list.size());
      for (const auto & item : list)
        send[i].push_back(_elem_data_vector[item]);

      // issue non-blocking send
      _communicator.send(pid, send[i], send_requests[i], send_tag);
    }

    // receive messages - we assume that we receive as many messages as we send!
    // bounding box overlapp is transitive, but data exhange between overlapping procs could still
    // be unidirectional!
    for (const auto i : index_range(_candidate_procs))
    {
      libmesh_ignore(i);

      // inspect incoming message
      Parallel::Status status(_communicator.probe(Parallel::any_source, send_tag));
      const auto source_pid = TIMPI::cast_int<processor_id_type>(status.source());
      const auto message_size = status.size(item_type);

      // resize receive buffer accordingly and receive data
      receive.resize(message_size);
      _communicator.receive(source_pid, receive, send_tag);

      // append communicated data
      _elem_data_vector.insert(_elem_data_vector.end(), receive.begin(), receive.end());
    }

    // wait until all send requests are at least buffered and we can destroy
    // the send buffers by going out of scope
    Parallel::wait(send_requests);
  }

  // build KD-Tree using data we just received
  const unsigned int max_leaf_size = 20; // slightly affects runtime
  auto point_list =
      PointListAdaptor<ElementData>(_elem_data_vector.begin(), _elem_data_vector.end());
  _kd_tree = std::make_unique<KDTreeType>(
      LIBMESH_DIM, point_list, nanoflann::KDTreeSingleIndexAdaptorParams(max_leaf_size));

  mooseAssert(_kd_tree != nullptr, "KDTree was not properly initialized.");
  _kd_tree->buildIndex();

  // build thread loop functor
  // ThreadedRadialAverageLoopTop88 rgcl(*this);

  // run threads
  auto local_range_begin = _elem_data_vector.begin();
  auto local_range_end = local_range_begin;
  std::advance(local_range_end, local_size);
  // Threads::parallel_reduce(ElementDataRange(local_range_begin, local_range_end), rgcl);
}

void
GatherElementData::meshChanged()
{
  // get underlying libMesh mesh
  auto & mesh = _mesh.getMesh();

  // Build a new node to element map
  _nodes_to_elem_map.clear();
  MeshTools::build_nodes_to_elem_map(_mesh.getMesh(), _nodes_to_elem_map);

  // clear procesor boundary nodes set
  _boundary_nodes.clear();

  //
  // iterate over active local elements and store all processor boundary node locations
  //
  const auto end = mesh.active_local_elements_end();
  for (auto it = mesh.active_local_elements_begin(); it != end; ++it)
    // find faces at processor boundaries
    for (const auto s : make_range((*it)->n_sides()))
    {
      const auto * neighbor = (*it)->neighbor_ptr(s);
      if (neighbor && neighbor->processor_id() != _my_pid)
        // add all nodes on the processor boundary
        for (const auto n : make_range((*it)->n_nodes()))
          if ((*it)->is_node_on_side(n, s))
            _boundary_nodes.insert((*it)->node_ref(n));

      // request communication list update
      _update_communication_lists = true;
    }
}

void
GatherElementData::updateCommunicationLists()
{
  Real _radius = 1.5;
  Real _padding = 1e-3;
  // clear communication lists
  _communication_lists.clear();
  _communication_lists.resize(n_processors());

  // build KD-Tree using local qpoint data
  const unsigned int max_leaf_size = 20; // slightly affects runtime
  auto point_list =
      PointListAdaptor<ElementData>(_elem_data_vector.begin(), _elem_data_vector.end());
  auto kd_tree = std::make_unique<KDTreeType>(
      LIBMESH_DIM, point_list, nanoflann::KDTreeSingleIndexAdaptorParams(max_leaf_size));
  mooseAssert(kd_tree != nullptr, "KDTree was not properly initialized.");
  kd_tree->buildIndex();

  std::vector<nanoflann::ResultItem<std::size_t, Real>> ret_matches;
  nanoflann::SearchParameters search_params;

  // iterate over all boundary nodes and collect all boundary-near data points
  _boundary_data_indices.clear();
  for (const auto & bn : _boundary_nodes)
  {
    ret_matches.clear();
    kd_tree->radiusSearch(
        &(bn(0)), Utility::pow<2>(_radius + _padding), ret_matches, search_params);
    for (auto & match : ret_matches)
      _boundary_data_indices.insert(match.first);
  }

  // gather all processor bounding boxes (communicate as pairs)
  std::vector<std::pair<Point, Point>> pps(n_processors());
  const auto mybb = _mesh.getInflatedProcessorBoundingBox(0);
  std::pair<Point, Point> mypp = mybb;
  _communicator.allgather(mypp, pps);

  // inflate all processor bounding boxes by radius (no padding)
  const auto rpoint = Point(1, 1, 1) * _radius;
  std::vector<BoundingBox> bbs;
  for (const auto & pp : pps)
    bbs.emplace_back(pp.first - rpoint, pp.second + rpoint);

  // get candidate processors (overlapping bounding boxes)
  _candidate_procs.clear();
  for (const auto pid : index_range(bbs))
    if (pid != _my_pid && bbs[pid].intersects(mypp))
      _candidate_procs.push_back(pid);

  // go over all boundary data items and send them to the proc they overlap with
  for (const auto i : _boundary_data_indices)
    for (const auto pid : _candidate_procs)
      if (bbs[pid].contains_point(_elem_data_vector[i].center))
        _communication_lists[pid].insert(i);

  // done
  _update_communication_lists = false;
}

namespace TIMPI
{

StandardType<GatherElementData::ElementData>::StandardType(
    const GatherElementData::ElementData * example)
{
  // We need an example for MPI_Address to use
  static const GatherElementData::ElementData p;
  if (!example)
    example = &p;

#ifdef LIBMESH_HAVE_MPI

  // Get the sub-data-types, and make sure they live long enough
  // to construct the derived type
  StandardType<Point> d1(&example->center);
  StandardType<Real> d2(&example->sensitivities);
  StandardType<Real> d3(&example->design_density);
  StandardType<Real> d4(&example->filtered_density);
  StandardType<Real> d5(&example->filtered_sensitivities);

  MPI_Datatype types[] = {
      (data_type)d1, (data_type)d2, (data_type)d3, (data_type)d4, (data_type)d5};
  int blocklengths[] = {1, 1, 1, 1, 1};
  MPI_Aint displs[5], start;

  libmesh_call_mpi(MPI_Get_address(const_cast<GatherElementData::ElementData *>(example), &start));
  libmesh_call_mpi(MPI_Get_address(const_cast<Point *>(&example->center), &displs[0]));
  libmesh_call_mpi(MPI_Get_address(const_cast<Real *>(&example->sensitivities), &displs[1]));
  libmesh_call_mpi(MPI_Get_address(const_cast<Real *>(&example->design_density), &displs[2]));
  libmesh_call_mpi(MPI_Get_address(const_cast<Real *>(&example->filtered_density), &displs[3]));
  libmesh_call_mpi(
      MPI_Get_address(const_cast<Real *>(&example->filtered_sensitivities), &displs[4]));

  for (std::size_t i = 0; i < 5; ++i)
    displs[i] -= start;

  // create a prototype structure
  MPI_Datatype tmptype;
  libmesh_call_mpi(MPI_Type_create_struct(5, blocklengths, displs, types, &tmptype));
  libmesh_call_mpi(MPI_Type_commit(&tmptype));

  // resize the structure type to account for padding, if any
  libmesh_call_mpi(
      MPI_Type_create_resized(tmptype, 0, sizeof(GatherElementData::ElementData), &_datatype));
  libmesh_call_mpi(MPI_Type_free(&tmptype));

  this->commit();

#endif // LIBMESH_HAVE_MPI
}

StandardType<GatherElementData::ElementData>::StandardType(
    const StandardType<GatherElementData::ElementData> & t)
  : DataType(t._datatype)
{
#ifdef LIBMESH_HAVE_MPI
  libmesh_call_mpi(MPI_Type_dup(t._datatype, &_datatype));
#endif
}

} // namespace TIMPI

void
GatherElementData::gatherElementData()
{
  TIME_SECTION("gatherElementData", 3, "Gathering Element Data");
  _elem_data_map.clear();

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();

      Point bl = elem->point(0);
      Point br = elem->point(1);
      Point tr = elem->point(2);
      Point tl = elem->point(3);

      std::vector<Real> sens_values(_n_vars);
      std::vector<Real> filt_values(_n_vars);
      int i = 0;
      for (auto & sensitivity : _sensitivities)
      {
        sens_values[i] =
            dynamic_cast<MooseVariableFE<Real> *>(sensitivity)->getElementalValue(elem);
        i++;
      }

      ElementData data = ElementData(
          tr,
          dynamic_cast<MooseVariableFE<Real> *>(_sensitivities[0])->getElementalValue(elem),
          dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem),
          0,
          0);
      _elem_data_map[elem_id] = data;
      _elem_data_vector.emplace_back(data);
    }

  std::cout << _elem_data_map.size() << "\n\n";
}
