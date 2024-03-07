//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "StressResponse.h"
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

#include <algorithm>

registerMooseObject("OptimizationApp", StressResponse);

InputParameters
StressResponse::validParams()
{
  InputParameters params = TopologyOptimizationDesignResponse::validParams();
  params.addClassDescription(
      "Computes the stress sensitivities for stress constrained topology optimization (2D ONLY).");
  params.addParam<std::vector<VariableName>>("stresses", "Stress names (VM, xx, yy, xy).");
  params.addParam<std::vector<VariableName>>("displacements",
                                             "Variable containing the x displacement");
  params.addRequiredParam<Real>("E0", "Real youngs modulus");
  params.addRequiredParam<Real>("Emin", "Minimum youngs modulus");
  params.addRequiredParam<Real>("poissons_ratio", "Poisson's ratio");
  params.addRequiredParam<Real>("p", "Penalty value");
  params.addParam<Real>("P", 12, "KS function parameter");
  params.addParam<Real>("q", 0.5, "Stress penalization parameter");
  params.addParam<std::string>(
      "system_matrix", "system", "The matrix tag name corresponding to the system matrix.");
  return params;
}

StressResponse::StressResponse(const InputParameters & parameters)
  : TopologyOptimizationDesignResponse(parameters),
    _dof_map(_sys.dofMap()),
    _stress_names(getParam<std::vector<VariableName>>("stresses")),
    _displacement_names(getParam<std::vector<VariableName>>("displacements")),
    _E0(getParam<Real>("E0")),
    _Emin(getParam<Real>("Emin")),
    _nu(getParam<Real>("poissons_ratio")),
    _p(getParam<Real>("p")),
    _P(getParam<Real>("P")),
    _q(getParam<Real>("q")),
    _system_matrix_tag_id(_subproblem.getMatrixTagID(getParam<std::string>("system_matrix")))
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
StressResponse::initialize()
{
  gatherNodalData();
  gatherElementData();
  computeStress();
  computeValue();
  computeSensitivity();
}

void
StressResponse::execute()
{
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
StressResponse::gatherNodalData()
{
  _nodal_data_map.clear();

  for (const auto & node : _mesh.getMesh().active_node_ptr_range())
  {
    dof_id_type node_id = node->id();

    std::vector<dof_id_type> dofs;
    dofs.push_back(node->dof_number(_sys.number(), 0, 0));
    dofs.push_back(node->dof_number(_sys.number(), 1, 0));

    NodalData data = NodalData(_disp_x->getNodalValue(*node), _disp_y->getNodalValue(*node), dofs);
    _nodal_data_map[node_id] = data;
  }
}

void
StressResponse::gatherElementData()
{
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

  _n_el = _elem_data_map.size();
}

void
StressResponse::computeStress()
{
  _stress.resize(_n_el, 3);
  _vonmises.resize(_n_el);
  initializeEMat();
  for (auto && [id, elem_data] : _elem_data_map)
  {
    RealEigenVector vector =
        std::pow(elem_data.physical_density, _q) * _E * getBMat(0, 0).transpose() * elem_data.u_el;
    _stress.row(id) << vector.transpose();
    _vonmises(id) = std::sqrt(std::pow(vector(0), 2) + std::pow(vector(1), 2) -
                              vector(0) * vector(1) + 3 * std::pow(vector(2), 2));
  }
}

void
StressResponse::computeValue()
{
  Real PM = 0;
  for (auto && [id, elem_data] : _elem_data_map)
  {
    if (_is_objective)
      PM += std::pow(_vonmises(id), _P);
    if (_is_constraint)
      PM += std::pow(_vonmises(id) / _limit, _P);
  }
  _value->reinit();
  PM = std::pow(1.0 / _n_el * PM, 1.0 / _P) - 1;
  _value->setValues(PM);
  _value->insert(_value->sys().solution());
  _value->sys().solution().close();
}

void
StressResponse::computeSensitivity()
{
  // all DOFs
  dof_id_type n_dofs = _sys.system().n_dofs();
  std::vector<dof_id_type> all_dofs(n_dofs);
  std::iota(std::begin(all_dofs), std::end(all_dofs), 0);

  // fixed DOFS
  std::vector<dof_id_type> fixed_dofs;
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
            fixed_dofs.push_back(node->dof_number(sys.number(), bc->variable().number() + c, 0));
    }
  }
  std::sort(std::begin(fixed_dofs), std::end(fixed_dofs));

  // free DOFs
  std::vector<dof_id_type> free_dofs;
  std::set_difference(std::begin(all_dofs),
                      std::end(all_dofs),
                      std::begin(fixed_dofs),
                      std::end(fixed_dofs),
                      std::inserter(free_dofs, std::begin(free_dofs)));

  // Elemen to DOF map
  std::vector<std::vector<dof_id_type>> elem_to_dof_map(_n_el);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    for (auto & node : elem_data.nodes)
    {
      elem_to_dof_map[id].insert(std::end(elem_to_dof_map[id]),
                                 std::begin(_nodal_data_map[node].dofs),
                                 std::end(_nodal_data_map[node].dofs));
    }
  }

  /// B-matrix and element stiffness matrix
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

  RealEigenMatrix KE(8, 8);
  for (int qp = 0; qp < 4; qp++)
  {
    Real xi = qps[qp].first;
    Real eta = qps[qp].second;
    RealEigenMatrix B = getBMat(xi, eta);
    Real K_qp_prefactor = t * det_J;
    RealEigenMatrix K_qp = K_qp_prefactor * B * _E * B.transpose();
    KE += K_qp;
  }

  /// Global displacement vector
  RealEigenVector U(n_dofs);
  for (auto && [id, node] : _nodal_data_map)
  {
    U(node.dofs[0]) = node.disp_x;
    U(node.dofs[1]) = node.disp_y;
  }

  /// dsigma^PM/dsigma_VMi
  RealEigenVector dPMdVM(_n_el);
  Real sum = 0;
  for (auto && [id, elem_data] : _elem_data_map)
  {
    sum += std::pow(_vonmises(id) / _limit, _P);
  }
  for (auto && [id, elem_data] : _elem_data_map)
  {
    dPMdVM(id) = 1.0 / _P * std::pow(1.0 / _n_el * sum, 1.0 / _P - 1) * _P * 1.0 / _n_el *
                 std::pow(_vonmises(id) / _limit, _P - 1) * 1 / _limit;
  }

  /// dsigma_VMi/dsigma_i
  std::vector<RealEigenVector> dVMdS(_n_el, RealEigenVector(3));
  for (auto && [id, elem_data] : _elem_data_map)
  {
    dVMdS[id](0) = 0.5 / _vonmises(id) * (2 * _stress(id, 0) - _stress(id, 1));
    dVMdS[id](1) = 0.5 / _vonmises(id) * (2 * _stress(id, 1) - _stress(id, 0));
    dVMdS[id](2) = 3 / _vonmises(id) * _stress(id, 2);
  }

  /// vector beta
  // NOTE: make q parameter
  Real q = 0.5;
  RealEigenVector beta(_n_el);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    RealEigenMatrix B = getBMat(0, 0);
    Real value = dVMdS[id].transpose() * _E * B.transpose() * _elem_data_map[id].u_el;
    beta(id) = q * std::pow(elem_data.physical_density, q - 1) * value;
  }

  /// vector gamma and gamma_red
  std::vector<Real> gamma(n_dofs), gamma_red(n_dofs);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    RealEigenVector vector = getBMat(0, 0) * _E * dVMdS[id];
    int x = 0;
    for (auto & dof : elem_to_dof_map[id])
    {
      gamma[dof] += dPMdVM(id) * std::pow(elem_data.physical_density, q) * vector(x);
      bool dof_is_free =
          std::find(std::begin(free_dofs), std::end(free_dofs), dof) != std::end(free_dofs);
      if (dof_is_free)
        gamma_red[dof] += dPMdVM(id) * std::pow(elem_data.physical_density, q) * vector(x);
      x++;
    }
  }

  /// vector lambda
  auto & solver = *static_cast<ImplicitSystem &>(_sys.system()).get_linear_solver();
  SparseMatrix<Number> & K = static_cast<ImplicitSystem &>(_sys.system()).get_system_matrix();
  PetscVector<Number> gamma_red_petsc(_communicator), lambda(_communicator);
  gamma_red_petsc.init(n_dofs);
  gamma_red_petsc = gamma_red;
  lambda.init(n_dofs);

  // std::cout << gamma_red_petsc << std::endl << std::endl << lambda_petsc << std::endl;

  solver.solve(K, K, lambda, gamma_red_petsc, 1e-3, 100);

  // std::cout << gamma_red_petsc << std::endl << std::endl << lambda_petsc << std::endl;

  /// final sensitivity
  RealEigenVector T1 = dPMdVM.cwiseProduct(beta);
  RealEigenVector T2(_n_el);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    RealEigenVector lambda_el(8);
    int x = 0;
    for (auto & dof : elem_to_dof_map[id])
    {
      lambda_el(x) = lambda(dof);
      x++;
    }
    Real value = lambda_el.transpose() * KE * _elem_data_map[id].u_el;
    T2(id) = -_p * std::pow(elem_data.physical_density, _p - 1) * (_E0 - _Emin) * value;
    elem_data.stress_sensitivity = T1(id) + T2(id);
  }
}

void
StressResponse::initializeEMat()
{
  // Elasticity tensor E
  Real E_prefactor = _E0 / (1 - std::pow(_nu, 2));
  RealEigenMatrix E{{1, _nu, 0}, {_nu, 1, 0}, {0, 0, (1 - _nu) / 2}};
  _E = E_prefactor * E;
}

RealEigenMatrix
StressResponse::getBMat(Real xi, Real eta)
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
