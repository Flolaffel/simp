//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "StressResponseBase.h"
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

InputParameters
StressResponseBase::validParams()
{
  InputParameters params = TODesignResponse::validParams();
  params.addClassDescription(
      "Computes the stress sensitivities for stress constrained topology optimization (2D ONLY).");
  params.addParam<std::vector<VariableName>>("stresses", "Stress names (VM, xx, yy, xy).");
  params.addParam<std::vector<VariableName>>("displacements",
                                             "Variable containing the x displacement");
  params.addRequiredParam<Real>("E0", "Real youngs modulus");
  params.addRequiredParam<Real>("Emin", "Minimum youngs modulus");
  params.addRequiredParam<Real>("poissons_ratio", "Poisson's ratio");
  params.addRequiredParam<Real>("p", "Penalty value");
  params.addParam<Real>("P", 12, "Aggregation function parameter");
  params.addParam<std::string>(
      "system_matrix", "system", "The matrix tag name corresponding to the system matrix.");
  return params;
}

StressResponseBase::StressResponseBase(const InputParameters & parameters)
  : TODesignResponse(parameters),
    _dof_map(_sys.dofMap()),
    _stress_names(getParam<std::vector<VariableName>>("stresses")),
    _displacement_names(getParam<std::vector<VariableName>>("displacements")),
    _E0(getParam<Real>("E0")),
    _Emin(getParam<Real>("Emin")),
    _nu(getParam<Real>("poissons_ratio")),
    _p(getParam<Real>("p")),
    _P(getParam<Real>("P"))
{
  if (isParamValid("stresses"))
    for (unsigned int i = 0; i < _stress_names.size(); i++)
    {
      if (_stress_names[i].find("xx") != std::string::npos)
        _stress_xx = &_subproblem.getStandardVariable(_tid, _stress_names[i]);
      else if (_stress_names[i].find("yy") != std::string::npos)
        _stress_yy = &_subproblem.getStandardVariable(_tid, _stress_names[i]);
      else if (_stress_names[i].find("xy") != std::string::npos)
        _stress_xy = &_subproblem.getStandardVariable(_tid, _stress_names[i]);
      else if (_stress_names[i].find("mises") != std::string::npos)
        _vonmises_stress = &_subproblem.getStandardVariable(_tid, _stress_names[i]);
    }

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
StressResponseBase::initialSetup()
{
  TIME_SECTION("initialSetup", 2, "Initial StressBase Setup");
  initializeDofVariables();
  initializeEMat();
  initializeKeMat();
}

void
StressResponseBase::initialize()
{
  TIME_SECTION("initialize", 2, "Initialize StressBase");
  gatherNodalData();
  gatherElementData();
  initializeUVec();
  computeStress();
  computeValue();
  computeSensitivity();
}

void
StressResponseBase::execute()
{
  TIME_SECTION("execute", 3, "Writing Stress Sensitivity");
  // Grab the element data for each id
  auto elem_data_iter = _elem_data_map.find(_current_elem->id());

  // Check if the element data is not null
  if (elem_data_iter != _elem_data_map.end())
  {
    ElementData & elem_data = elem_data_iter->second;
    dynamic_cast<MooseVariableFE<Real> *>(_sensitivity)
        ->setNodalValue(elem_data.stress_sensitivity);
  }
  else
  {
    mooseError("Element data not found for the current element id.");
  }
}

void
StressResponseBase::gatherNodalData()
{
  TIME_SECTION("gatherNodalData", 3, "Gathering Nodal Data");
  _nodal_data_map.clear();

  for (const auto & node : _mesh.getMesh().active_node_ptr_range())
  {
    dof_id_type node_id = node->id();

    std::vector<dof_id_type> dofs;
    _dof_map.dof_indices(node, dofs);

    NodalData data = NodalData(_disp_x->getNodalValue(*node), _disp_y->getNodalValue(*node), dofs);
    _nodal_data_map[node_id] = data;
  }
}

void
StressResponseBase::gatherElementData()
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
        _elem_to_node_map[elem->id()].push_back(elem->node_id(n));
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
          dynamic_cast<MooseVariableFE<Real> *>(_vonmises_stress)->getElementalValue(elem),
          dynamic_cast<MooseVariableFE<Real> *>(_stress_xx)->getElementalValue(elem),
          dynamic_cast<MooseVariableFE<Real> *>(_stress_yy)->getElementalValue(elem),
          dynamic_cast<MooseVariableFE<Real> *>(_stress_xy)->getElementalValue(elem),
          dynamic_cast<const MooseVariableFE<Real> *>(_physical_density)->getElementalValue(elem),
          0,
          nodes,
          u_el);
      _elem_data_map[elem_id] = data;
    }
}

void
StressResponseBase::initializeDofVariables()
{
  TIME_SECTION("initializeDofVariables", 6, "Initializing DOF Variables");
  // all DOFs
  _n_dofs = _sys.system().n_dofs();
  _all_dofs.resize(_n_dofs);
  std::iota(std::begin(_all_dofs), std::end(_all_dofs), 0);

  // fixed DOFS
  NonlinearSystemBase & sys = static_cast<NonlinearSystemBase &>(_sys);
  auto & nbc_warehouse = sys.getNodalBCWarehouse();
  if (nbc_warehouse.hasActiveObjects())
  {
    for (const auto & bnode : *_mesh.getBoundaryNodeRange())
    {
      BoundaryID boundary_id = bnode->_bnd_id;
      Node * node = bnode->_node;

      if (!nbc_warehouse.hasActiveBoundaryObjects(boundary_id) ||
          node->processor_id() != processor_id())
        continue;

      for (const auto & bc : nbc_warehouse.getActiveBoundaryObjects(boundary_id))
        if (bc->shouldApply())
          for (unsigned int c = 0; c < bc->variable().count(); ++c)
            _fixed_dofs.push_back(node->dof_number(sys.number(), bc->variable().number() + c, 0));
    }
  }
  std::sort(std::begin(_fixed_dofs), std::end(_fixed_dofs));

  // free DOFs
  std::set_difference(std::begin(_all_dofs),
                      std::end(_all_dofs),
                      std::begin(_fixed_dofs),
                      std::end(_fixed_dofs),
                      std::inserter(_free_dofs, std::begin(_free_dofs)));

  // Elemen to DOF map
  _n_el = _mesh.nElem();
  _elem_to_dof_map.resize(_n_el);
  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      std::vector<dof_id_type> dofs;
      for (auto & node : elem->node_ref_range())
      {
        dofs.push_back(node.dof_number(_sys.number(), 0, 0));
        dofs.push_back(node.dof_number(_sys.number(), 1, 0));
      }
      dof_id_type elem_id = elem->id();
      _elem_to_dof_map[elem_id] = dofs;
    }
}

RealEigenMatrix
StressResponseBase::getBMat(Real xi, Real eta)
{
  // only true for unit element size
  int l = 1;
  Real B_prefactor = 1.0 / (2 * l);
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
StressResponseBase::initializeEMat()
{
  TIME_SECTION("initializeEMat", 6, "Initializing Elasticity Matrix");
  // Elasticity tensor E
  Real E_prefactor = _E0 / (1 - std::pow(_nu, 2));
  RealEigenMatrix E{{1, _nu, 0}, {_nu, 1, 0}, {0, 0, (1 - _nu) / 2}};
  _E = E_prefactor * E;
}

void
StressResponseBase::initializeKeMat()
{
  TIME_SECTION("initializeKeMat", 6, "Initializing Element Stiffness Matrix");
  /// Element stiffness matrix
  _KE.resize(8, 8);

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
  std::vector<std::vector<Real>> J{{0.5, 0}, {0, 0.5}};
  Real det_J = J[0][0] * J[1][1] - J[0][1] * J[1][0];

  for (int qp = 0; qp < 4; qp++)
  {
    Real xi = qps[qp].first;
    Real eta = qps[qp].second;
    RealEigenMatrix B = getBMat(xi, eta);
    Real K_qp_prefactor = t * det_J;
    RealEigenMatrix K_qp = K_qp_prefactor * B * _E * B.transpose();
    _KE += K_qp;
  }
}

void
StressResponseBase::initializeUVec()
{
  TIME_SECTION("initializeUVec", 6, "Initializing Global Displacement Vector");
  /// Global displacement vector
  _U.resize(_n_dofs);
  for (auto && [id, node] : _nodal_data_map)
  {
    _U(node.dofs[0]) = node.disp_x;
    _U(node.dofs[1]) = node.disp_y;
  }
}

RealEigenVector
StressResponseBase::getLambda(std::vector<Real> gamma, std::vector<dof_id_type> fixed_dofs)
{
  TIME_SECTION("getLambda", 4, "Computing Lambda");
  /// vector lambda
  auto & solver = *static_cast<ImplicitSystem &>(_sys.system()).get_linear_solver();
  SparseMatrix<Number> & K = static_cast<ImplicitSystem &>(_sys.system()).get_system_matrix();
  PetscVector<Number> gamma_red(_communicator, _n_dofs), lambda_petsc(_communicator, _n_dofs);
  gamma_red = gamma;

  std::vector<PetscScalar> zeros(fixed_dofs.size());
  VecSetValues(gamma_red.vec(),
               cast_int<PetscInt>(fixed_dofs.size()),
               numeric_petsc_cast(fixed_dofs.data()),
               zeros.data(),
               INSERT_VALUES);

  solver.solve(K, K, lambda_petsc, gamma_red, 1e-8, 100);

  RealEigenVector lambda(_n_dofs);
  for (unsigned int i = 0; i < _n_dofs; i++)
    lambda(i) = lambda_petsc(i);

  return lambda;
}

RealEigenVector
StressResponseBase::getT2(RealEigenVector lambda)
{
  TIME_SECTION("getT2", 4, "Computing T2");
  /// final sensitivity
  RealEigenVector T2(_n_el);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    RealEigenVector lambda_el(8);
    int x = 0;
    for (auto & dof : _elem_to_dof_map[id])
    {
      lambda_el(x) = lambda(dof);
      x++;
    }
    Real value = lambda_el.transpose() * _KE * _elem_data_map[id].u_el;
    T2(id) = -_p * std::pow(elem_data.physical_density, _p - 1) * (_E0 - _Emin) * value;
  }
  return T2;
}
