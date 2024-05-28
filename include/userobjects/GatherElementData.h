//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ElementUserObject.h"
#include "MooseTypes.h"

#include "PointListAdaptor.h"

#include "ElementUserObject.h"
#include "DataIO.h"
#include "PointListAdaptor.h"
#include "Function.h"

#include "libmesh/data_type.h"

#include <set>
#include <map>
#include <vector>
#include <memory>

/**
 * Element user object that filters the objective function (and constraint) sensitivities, depending
 * on filter type.
 */

class GatherElementData : public ElementUserObject
{
public:
  static InputParameters validParams();

  GatherElementData(const InputParameters & parameters);

  virtual void initialSetup() override;

  virtual void initialize() override;
  virtual void execute() override;
  virtual void threadJoin(const UserObject & y) override;
  virtual void finalize() override;
  virtual void meshChanged() override;

  struct ElementData
  {
    Point centroid;
    Real sensitivities;
    Real design_density;
    Real filtered_density;
    Real filtered_sensitivities;
    ElementData()
      : centroid(0),
        sensitivities(0),
        design_density(0),
        filtered_density(0),
        filtered_sensitivities(0)
    {
    }
    ElementData(Point p, Real sens, Real dens, Real filt_dens, Real filt_sens)
      : centroid(p),
        sensitivities(sens),
        design_density(dens),
        filtered_density(filt_dens),
        filtered_sensitivities(filt_sens)
    {
    }
  };

  const std::map<dof_id_type, ElementData> & getMap() const { return _elem_data_map; }
  const std::vector<ElementData> & getVector() const { return _elem_data_vector; }

protected:
  /// Number of variables
  unsigned int _n_vars;
  /// Variable vector
  std::vector<MooseWritableVariable *> _sensitivities;
  /// Design density variable name
  VariableName _design_density_name;
  /// The design density variable
  MooseVariable * _design_density;
  /// Filtered density variable name
  VariableName _filtered_density_name;
  /// Filtered density variable
  MooseVariable * _filtered_density;

private:
  /// Data structure to hold element data
  std::map<dof_id_type, ElementData> _elem_data_map;
  std::vector<ElementData> _elem_data_vector;
  std::vector<ElementData> _elem_data_vector_test;

  /**
   * Gathers element data
   */
  void gatherElementData();

  void updateCommunicationLists();

  /// QPData indices to send to the various processors
  std::vector<std::set<std::size_t>> _communication_lists;
  bool _update_communication_lists;

  /// processors to send (potentially empty) data to
  std::vector<processor_id_type> _candidate_procs;

  processor_id_type _my_pid;

  using KDTreeType = nanoflann::KDTreeSingleIndexAdaptor<
      nanoflann::L2_Simple_Adaptor<Real, PointListAdaptor<ElementData>>,
      PointListAdaptor<ElementData>,
      LIBMESH_DIM,
      std::size_t>;

  /// spatial index (nanoflann guarantees this to be threadsafe under read-only operations)
  std::unique_ptr<KDTreeType> _kd_tree;

  /// The data structure used to find neighboring elements give a node ID
  std::vector<std::vector<const Elem *>> _nodes_to_elem_map;

  /// set of nodes on the boundary of the current processor domain
  std::set<Point> _boundary_nodes;

  /// set of all _qp_data indices that are within _radius of any _boundary_nodes
  std::set<std::size_t> _boundary_data_indices;
};

namespace TIMPI
{

template <>
class StandardType<GatherElementData::ElementData> : public DataType
{
public:
  explicit StandardType(const GatherElementData::ElementData * example = nullptr);
  StandardType(const StandardType<GatherElementData::ElementData> & t);
  ~StandardType() { this->free(); }
};

} // namespace TIMPI
