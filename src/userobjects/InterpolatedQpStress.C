//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "InterpolatedQpStress.h"
#include "NonlinearSystemBase.h"
#include "DirichletBCBase.h"

registerMooseObject("OptimizationApp", InterpolatedQpStress);

InputParameters
InterpolatedQpStress::validParams()
{
  InputParameters params = ElementUserObject::validParams();
  params.addClassDescription("Computes the stress sensitivities for stress constrained topology "
                             "optimization using qp-relaxation and P-norm aggregation (2D ONLY).");
  params.addParam<Real>("q", 0.5, "Stress penalization parameter");
  params.addParam<VariableName>("physical_density", "Physical density variable name.");
  params.addRequiredParam<Real>("E0", "Real youngs modulus");
  params.addRequiredParam<Real>("Emin", "Minimum youngs modulus");
  params.addRequiredParam<Real>("poissons_ratio", "Poisson's ratio");
  params.addRequiredParam<Real>("p", "Penalty value");
  params.addRequiredParam<std::vector<VariableName>>("displacements",
                                                     "Variable containing the x displacement");
  params.addRequiredCoupledVar("interpolated_micro_vonmises_stress", "Interpolated stress.");
  params.addRequiredCoupledVar("micro_vonmises_stress", "Von mises stress.");
  params.addRequiredParam<MeshGeneratorName>(
      "mesh_generator",
      "Name of the mesh generator to be used to retrieve control drums information.");
  return params;
}

InterpolatedQpStress::InterpolatedQpStress(const InputParameters & parameters)
  : ElementUserObject(parameters),
    _q(getParam<Real>("q")),
    _displacement_names(getParam<std::vector<VariableName>>("displacements")),
    _physical_density_name(getParam<VariableName>("physical_density")),
    _physical_density(&_subproblem.getStandardVariable(_tid, _physical_density_name)),
    _E0(getParam<Real>("E0")),
    _Emin(getParam<Real>("Emin")),
    _nu(getParam<Real>("poissons_ratio")),
    _p(getParam<Real>("p")),
    _interpolated_micro_vonmises_stress(&writableVariable("interpolated_micro_vonmises_stress")),
    _micro_vonmises_stress(&writableVariable("micro_vonmises_stress")),
    _mesh_generator(getParam<MeshGeneratorName>("mesh_generator"))
{
  _nx = getMeshProperty<unsigned int>("num_elements_x", _mesh_generator);
  _ny = getMeshProperty<unsigned int>("num_elements_y", _mesh_generator);
  _xmin = getMeshProperty<Real>("xmin", _mesh_generator);
  _xmax = getMeshProperty<Real>("xmax", _mesh_generator);
  _ymin = getMeshProperty<Real>("ymin", _mesh_generator);
  _ymax = getMeshProperty<Real>("ymax", _mesh_generator);
  _n_el = _nx * _ny;
  _l_el = (_xmax - _xmin) / _nx;

  if (isParamValid("displacements"))
    for (unsigned int i = 0; i < _displacement_names.size(); i++)
    {
      if (_displacement_names[i] == "disp_x")
        _disp_x = &_subproblem.getStandardVariable(_tid, _displacement_names[i]);
      else if (_displacement_names[i] == "disp_y")
        _disp_y = &_subproblem.getStandardVariable(_tid, _displacement_names[i]);
    }
  else
    mooseError("Couldn't get displacements.");
}

void
InterpolatedQpStress::initialSetup()
{
  TIME_SECTION("initialSetup", 2, "Initial StressBase Setup");
  initializeDofVariables();
  initializeEMat();
}

void
InterpolatedQpStress::initialize()
{
  TIME_SECTION("initialize", 2, "Initialize StressBase");
  gatherNodalData();
  gatherElementData();
  initializeUVec();
  computeStress();
}

void
InterpolatedQpStress::execute()
{
  if (isParamValid("interpolated_micro_vonmises_stress"))
    dynamic_cast<MooseVariableFE<Real> *>(_interpolated_micro_vonmises_stress)
        ->setNodalValue(_interpolated_micro_vonmises(_current_elem->id()));

  if (isParamValid("micro_vonmises_stress"))
    dynamic_cast<MooseVariableFE<Real> *>(_micro_vonmises_stress)
        ->setNodalValue(_micro_vonmises(_current_elem->id()));
}

void
InterpolatedQpStress::initializeDofVariables()
{
  TIME_SECTION("initializeDofVariables", 6, "Initializing DOF Variables");
  // all DOFs
  _n_dofs = _sys.system().n_dofs();
  _n_local_dofs = _sys.system().n_local_dofs();
  _all_dofs.resize(_n_dofs);
  std::iota(std::begin(_all_dofs), std::end(_all_dofs), 0);

  // fixed DOFS
  NonlinearSystemBase & sys = static_cast<NonlinearSystemBase &>(_sys);
  NumericVector<Number> & initial_solution(_sys.solution());
  auto & nbc_warehouse = sys.getPresetNodalBCWarehouse();
  if (nbc_warehouse.hasActiveObjects())
  {
    for (const auto & bnode : *_mesh.getBoundaryNodeRange())
    {
      BoundaryID boundary_id = bnode->_bnd_id;
      Node * node = bnode->_node;

      if (!nbc_warehouse.hasActiveBoundaryObjects(boundary_id) ||
          node->processor_id() != processor_id())
        continue;

      _fe_problem.reinitNodeFace(node, boundary_id, 0);

      for (const auto & bc : nbc_warehouse.getActiveBoundaryObjects(boundary_id))
        if (bc->shouldApply())
          for (unsigned int c = 0; c < bc->variable().count(); ++c)
          {
            dof_id_type dof = node->dof_number(sys.number(), bc->variable().number() + c, 0);
            bc->computeValue(initial_solution);
            if (initial_solution(dof) == 0)
              _fixed_dofs.push_back(dof);
          }
    }
  }

  // Elemen to DOF map
  _elem_to_dof_map.resize(_n_el);
  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      std::vector<dof_id_type> dofs;
      for (auto & node : elem->node_ref_range())
      {
        Node * node_ptr = &node;
        // _dof_map.dof_indices(node_ptr, dofs);
        // NOTE: 2D only
        dofs.push_back(node.dof_number(_sys.number(), 0, 0));
        dofs.push_back(node.dof_number(_sys.number(), 1, 0));
      }
      dof_id_type elem_id = elem->id();
      _elem_to_dof_map[elem_id] = dofs;
    }
}

void
InterpolatedQpStress::initializeEMat()
{
  TIME_SECTION("initializeEMat", 6, "Initializing Elasticity Matrix");
  // Elasticity tensor E
  Real E_prefactor = _E0 / (1 - std::pow(_nu, 2));
  RealEigenMatrix E{{1, _nu, 0}, {_nu, 1, 0}, {0, 0, (1 - _nu) / 2}};
  _E = E_prefactor * E;
}

void
InterpolatedQpStress::gatherNodalData()
{
  TIME_SECTION("gatherNodalData", 3, "Gathering Nodal Data");
  _nodal_data_map.clear();

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      for (auto & node : elem->node_ref_range())
      {
        dof_id_type node_id = node.id();
        // NOTE: 2D only
        std::vector<dof_id_type> dofs;
        dofs.push_back(node.dof_number(_sys.number(), 0, 0));
        dofs.push_back(node.dof_number(_sys.number(), 1, 0));

        NodalData data =
            NodalData(_disp_x->getNodalValue(node), _disp_y->getNodalValue(node), dofs);
        _nodal_data_map[node_id] = data;
      }
    }
}

void
InterpolatedQpStress::gatherElementData()
{
  TIME_SECTION("gatherElementData", 3, "Gathering Element Data");
  _elem_data_map.clear();

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();

      std::vector<dof_id_type> nodes;
      for (unsigned int n = 0; n < elem->n_nodes(); n++)
      {
        nodes.push_back(elem->node_id(n));
      }

      // NOTE: 8 for QUAD4 elements
      RealEigenVector u_el(8);
      int x = 0;
      for (auto & node : nodes)
      {
        u_el(2 * x) = _nodal_data_map[node].disp_x;
        u_el(2 * x + 1) = _nodal_data_map[node].disp_y;
        x++;
      }

      ElementData data = ElementData(
          dynamic_cast<const MooseVariableFE<Real> *>(_physical_density)->getElementalValue(elem),
          u_el);
      _elem_data_map[elem_id] = data;
    }
}

void
InterpolatedQpStress::initializeUVec()
{
  TIME_SECTION("initializeUVec", 6, "Initializing Global Displacement Vector");
  // Global displacement vector
  // NOTE: 2D ONLY
  _U.resize(_n_dofs);
  _U.setZero();
  std::vector<std::pair<dof_id_type, Real>> U;
  for (auto && [id, node] : _nodal_data_map)
  {
    _U(node.dofs[0]) = node.disp_x;
    _U(node.dofs[1]) = node.disp_y;
  }
}

RealEigenMatrix
InterpolatedQpStress::computeBMat(Real xi, Real eta)
{
  Real B_prefactor = 1.0 / (2 * _l_el);
  RealEigenMatrix B{{eta - 1, 0, xi - 1},
                    {0, xi - 1, eta - 1},
                    {1 - eta, 0, -1 - xi},
                    {0, -1 - xi, 1 - eta},
                    {1 + eta, 0, 1 + xi},
                    {0, 1 + xi, 1 + eta},
                    {-1 - eta, 0, 1 - xi},
                    {0, 1 - xi, -1 - eta}};
  return B_prefactor * B;
}

void
InterpolatedQpStress::computeStress()
{
  TIME_SECTION("computeStress", 3, "Computing stress at element center");

  //_interpolated_micro_vonmises_old = _interpolated_micro_vonmises;

  _stress.resize(_n_el, 3);
  _stress.setZero();
  _interpolated_micro_vonmises.resize(_n_el);
  _interpolated_micro_vonmises.setZero();
  _micro_vonmises.resize(_n_el);
  _micro_vonmises.setZero();
  for (auto && [id, elem_data] : _elem_data_map)
  {
    RealEigenVector u_el = _U(_elem_to_dof_map[id]);
    RealEigenVector vector = _E * computeBMat(0, 0).transpose() * u_el;
    RealEigenVector interpolated_vector = std::pow(elem_data.physical_density, _q) * vector;
    _stress.row(id) << interpolated_vector.transpose();
    _interpolated_micro_vonmises(id) = std::sqrt(
        std::pow(interpolated_vector(0), 2) + std::pow(interpolated_vector(1), 2) -
        interpolated_vector(0) * interpolated_vector(1) + 3 * std::pow(interpolated_vector(2), 2));
    _micro_vonmises(id) = std::sqrt(std::pow(vector(0), 2) + std::pow(vector(1), 2) -
                                    vector(0) * vector(1) + 3 * std::pow(vector(2), 2));
  }
}
