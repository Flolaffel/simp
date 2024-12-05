//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ComplianceResponse.h"
#include "MooseError.h"
#include "NonlinearSystemBase.h"
#include "NodalBCBase.h"
#include "MooseVariableScalar.h"
#include "vector.h"

#include "libmesh/petsc_matrix.h"
#include "libmesh/sparse_matrix.h"

#include "libmesh/linear_implicit_system.h"
#include "libmesh/numeric_vector.h"
#include "libmesh/petsc_vector.h"
#include "libmesh/mesh_tools.h"
#include "libmesh/dof_map.h"

#include <algorithm>

#include "NodalConstraint.h"
#include "DirichletBCBase.h"
#include "SystemBase.h"

registerMooseObject("OptimizationApp", ComplianceResponse);

InputParameters
ComplianceResponse::validParams()
{
  InputParameters params = TODesignResponse::validParams();
  params.addClassDescription(
      "Computes the compliance sensitivities for topology optimization (2D ONLY).");
  params.addRequiredParam<std::vector<VariableName>>("displacements",
                                                     "Variable containing the x displacement");
  params.addRequiredParam<Real>("E0", "Real youngs modulus");
  params.addRequiredParam<Real>("Emin", "Minimum youngs modulus");
  params.addRequiredParam<Real>("poissons_ratio", "Poisson's ratio");
  params.addRequiredParam<Real>("p", "Penalty value");
  // params.addRequiredParam<std::string>("system_matrix",
  //                                      "The matrix tag name corresponding to the system
  //                                      matrix.");
  params.addRequiredParam<MeshGeneratorName>("mesh_generator",
                                             "Name of the mesh generator to be used to retrieve "
                                             "control drums information.");
  return params;
}

ComplianceResponse::ComplianceResponse(const InputParameters & parameters)
  : TODesignResponse(parameters),
    _dof_map(_sys.dofMap()),
    _mesh_generator(getParam<MeshGeneratorName>("mesh_generator")),
    _displacement_names(getParam<std::vector<VariableName>>("displacements")),
    _E0(getParam<Real>("E0")),
    _Emin(getParam<Real>("Emin")),
    _nu(getParam<Real>("poissons_ratio")),
    _p(getParam<Real>("p"))
// _system_matrix_name(getParam<std::string>("system_matrix")),
{
  _nx = getMeshProperty<unsigned int>("num_elements_x", _mesh_generator);
  _ny = getMeshProperty<unsigned int>("num_elements_y", _mesh_generator);
  _xmin = getMeshProperty<Real>("xmin", _mesh_generator);
  _xmax = getMeshProperty<Real>("xmax", _mesh_generator);
  _ymin = getMeshProperty<Real>("ymin", _mesh_generator);
  _ymax = getMeshProperty<Real>("ymax", _mesh_generator);

  _l_el = (_xmax - _xmin) / _nx;
  if (_l_el - (_ymax - _ymin) / _ny > 1e-3)
    mooseError("Please use quadratic elements for topology optimization.");

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
ComplianceResponse::initialSetup()
{
  TIME_SECTION("initialSetup", 2, "Initial StressBase Setup");
  initializeDofVariables();
  initializeEMat();
  initializeKeMat();
}

void
ComplianceResponse::initialize()
{
  TIME_SECTION("initialize", 2, "Initialize StressBase");
  gatherNodalData();
  gatherElementData();
  initializeUVec();
  computeValue();
  computeSensitivity();
}

void
ComplianceResponse::execute()
{
  // Grab the element data for each id
  auto elem_data_iter = _elem_data_map.find(_current_elem->id());

  // Check if the element data is not null
  mooseAssert(elem_data_iter != _elem_data_map.end(),
              "Element data not found for the current element id.");

  ElementData & elem_data = elem_data_iter->second;
  dynamic_cast<MooseVariableFE<Real> *>(_sensitivity)
      ->setNodalValue(elem_data.compliance_sensitivity);
}

void
ComplianceResponse::initializeDofVariables()
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
        // NOTE: 2D only
        dofs.push_back(node.dof_number(_sys.number(), 0, 0));
        dofs.push_back(node.dof_number(_sys.number(), 1, 0));
      }
      dof_id_type elem_id = elem->id();
      _elem_to_dof_map[elem_id] = dofs;
    }
}

void
ComplianceResponse::initializeKeMat()
{
  TIME_SECTION("initializeKeMat", 6, "Initializing Element Stiffness Matrix");
  /// Element stiffness matrix
  _KE.resize(8, 8);
  _KE.setZero();

  std::vector<std::pair<Real, Real>> qps{
      std::make_pair(-1, -1), std::make_pair(1, -1), std::make_pair(-1, 1), std::make_pair(1, 1)};
  for (int i = 0; i < 4; i++)
  {
    Real qps_prefactor = 1 / std::sqrt(3);
    qps[i].first *= qps_prefactor;
    qps[i].second *= qps_prefactor;
  }

  // Jacobi-matrix for unit thickness
  int t = 1;
  std::vector<std::vector<Real>> J{{_l_el / 2, 0}, {0, _l_el / 2}};
  Real det_J = J[0][0] * J[1][1] - J[0][1] * J[1][0];

  for (int qp = 0; qp < 4; qp++)
  {
    Real xi = qps[qp].first;
    Real eta = qps[qp].second;
    RealEigenMatrix B = computeBMat(xi, eta);
    Real K_qp_prefactor = t * det_J;
    RealEigenMatrix K_qp = K_qp_prefactor * B * _E * B.transpose();
    _KE += K_qp;
  }
}

void
ComplianceResponse::gatherNodalData()
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
ComplianceResponse::gatherElementData()
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
          0);
      _elem_data_map[elem_id] = data;
    }
}

void
ComplianceResponse::initializeUVec()
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

void
ComplianceResponse::initializeEMat()
{
  TIME_SECTION("initializeEMat", 6, "Initializing Elasticity Matrix");
  // Elasticity tensor E
  Real E_prefactor = _E0 / (1 - std::pow(_nu, 2));
  RealEigenMatrix E{{1, _nu, 0}, {_nu, 1, 0}, {0, 0, (1 - _nu) / 2}};
  _E = E_prefactor * E;
}

RealEigenMatrix
ComplianceResponse::computeBMat(Real xi, Real eta)
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
ComplianceResponse::computeValue()
{
  TIME_SECTION("computeValue", 3, "Computing P-norm value");
  Real c = 0;
  for (auto && [id, elem_data] : _elem_data_map)
  {
    RealEigenVector u_el = _U(_elem_to_dof_map[id]);
    Real E_el = _Emin + std::pow(elem_data.physical_density, _p) * (_E0 - _Emin);
    c += E_el * u_el.transpose() * _KE * u_el;
  }
  _communicator.sum(c);

  if (_is_objective)
    _value = c;
  if (_is_constraint)
    _value = c / _limit - 1;
  _scalar_value->reinit();
  _scalar_value->setValues(_value);
  _scalar_value->insert(_scalar_value->sys().solution());
  _scalar_value->sys().solution().close();
}

void
ComplianceResponse::computeSensitivity()
{
  TIME_SECTION("computeSensitivity", 3, "Computing Stress Sensitivity");

  if (_is_objective)
    _limit = 1;

  for (auto && [id, elem_data] : _elem_data_map)
  {
    RealEigenVector u_el = _U(_elem_to_dof_map[id]);
    // elem_data.compliance_sensitivity = 1 / (_l_el * _l_el * _limit) * -_p *
    //                                    std::pow(elem_data.physical_density, _p - 1) *
    //                                    (_E0 - _Emin) * u_el.transpose() * _KE * u_el;
    elem_data.compliance_sensitivity = 1 / _limit * -_p *
                                       std::pow(elem_data.physical_density, _p - 1) *
                                       (_E0 - _Emin) * u_el.transpose() * _KE * u_el;
  }
}
