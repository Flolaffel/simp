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

#include "NodalConstraint.h"
#include "DirichletBCBase.h"
#include "SystemBase.h"

InputParameters
StressResponseBase::validParams()
{
  InputParameters params = TODesignResponse::validParams();
  params.addClassDescription(
      "Computes the stress sensitivities for stress constrained topology optimization (2D ONLY).");
  params.addParam<std::vector<VariableName>>("stresses", "Stress names (VM, xx, yy, xy).");
  params.addCoupledVar("interpolated_micro_vonmises_stress", "Interpolated stress.");
  params.addCoupledVar("micro_vonmises_stress", "Von mises stress.");
  params.addRequiredParam<std::vector<VariableName>>("displacements",
                                                     "Variable containing the x displacement");
  params.addRequiredParam<Real>("E0", "Real youngs modulus");
  params.addRequiredParam<Real>("Emin", "Minimum youngs modulus");
  params.addRequiredParam<Real>("poissons_ratio", "Poisson's ratio");
  params.addRequiredParam<Real>("p", "Penalty value");
  params.addParam<Real>("P", 12, "Aggregation function parameter");
  params.addRequiredParam<std::string>("system_matrix",
                                       "The matrix tag name corresponding to the system matrix.");
  params.addRequiredParam<MeshGeneratorName>(
      "mesh_generator",
      "Name of the mesh generator to be used to retrieve control drums information.");
  return params;
}

StressResponseBase::StressResponseBase(const InputParameters & parameters)
  : TODesignResponse(parameters),
    _dof_map(_sys.dofMap()),
    _displacement_names(getParam<std::vector<VariableName>>("displacements")),
    _E0(getParam<Real>("E0")),
    _Emin(getParam<Real>("Emin")),
    _nu(getParam<Real>("poissons_ratio")),
    _p(getParam<Real>("p")),
    _P(getParam<Real>("P")),
    _system_matrix_name(getParam<std::string>("system_matrix")),
    _mesh_generator(getParam<MeshGeneratorName>("mesh_generator"))
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

  if (isParamValid("stresses"))
  {
    _stress_names = getParam<std::vector<VariableName>>("stresses");
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

  if (isParamValid("interpolated_micro_vonmises_stress"))
    _interpolated_micro_vonmises_stress = &writableVariable("interpolated_micro_vonmises_stress");
  if (isParamValid("micro_vonmises_stress"))
    _micro_vonmises_stress = &writableVariable("micro_vonmises_stress");
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
  // Grab the element data for each id
  auto elem_data_iter = _elem_data_map.find(_current_elem->id());

  // Check if the element data is not null
  mooseAssert(elem_data_iter != _elem_data_map.end(),
              "Element data not found for the current element id.");

  ElementData & elem_data = elem_data_iter->second;
  dynamic_cast<MooseVariableFE<Real> *>(_sensitivity)->setNodalValue(elem_data.stress_sensitivity);

  if (isParamValid("interpolated_micro_vonmises_stress"))
    dynamic_cast<MooseVariableFE<Real> *>(_interpolated_micro_vonmises_stress)
        ->setNodalValue(_interpolated_micro_vonmises(_current_elem->id()));

  if (isParamValid("micro_vonmises_stress"))
    dynamic_cast<MooseVariableFE<Real> *>(_micro_vonmises_stress)
        ->setNodalValue(_micro_vonmises(_current_elem->id()));
}

void
StressResponseBase::initializeDofVariables()
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
StressResponseBase::gatherNodalData()
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
          u_el);
      _elem_data_map[elem_id] = data;
    }
}

void
StressResponseBase::initializeUVec()
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
StressResponseBase::scaleConstraint()
{
  std::cout << "old limit = " << _scaled_limit;
  // if (_scaling && _t_step % 10 == 0)
  if (_scaling)
  {
    _scaled_limit =
        std::max(_limit, _limit / _interpolated_micro_vonmises.maxCoeff() * (_value + 1) * _limit);
  }
  else if (!_scaling)
    _scaled_limit = _limit;
  std::cout << ". new limit = " << _scaled_limit << std::endl;
}

RealEigenVector
StressResponseBase::computeLambda(std::vector<Real> gamma)
{
  TIME_SECTION("computeLambda", 4, "Computing Lambda");
  // vector lambda
  auto & solver = *static_cast<ImplicitSystem &>(_sys.system()).get_linear_solver();
  auto & jacobian = _sys.getMatrix(_subproblem.getMatrixTagID(_system_matrix_name));

  PetscVector<Number> gamma_red(_communicator, _n_dofs, _n_local_dofs),
      lambda_petsc(_communicator, _n_dofs, _n_local_dofs);
  gamma_red = gamma;

  std::vector<PetscScalar> zeros(_fixed_dofs.size());
  LIBMESH_CHKERR(VecSetValues(gamma_red.vec(),
                              cast_int<PetscInt>(_fixed_dofs.size()),
                              numeric_petsc_cast(_fixed_dofs.data()),
                              zeros.data(),
                              INSERT_VALUES));

  auto jacobian_petsc = dynamic_cast<PetscMatrix<Number> *>(&jacobian);
  LIBMESH_CHKERR(MatZeroRowsColumns(jacobian_petsc->mat(),
                                    cast_int<PetscInt>(_fixed_dofs.size()),
                                    numeric_petsc_cast(_fixed_dofs.data()),
                                    1.0,
                                    NULL,
                                    NULL));

  solver.solve(jacobian, jacobian, lambda_petsc, gamma_red, 1e-8, 100);

  std::vector<Real> lambda(_n_dofs);
  lambda_petsc.localize(lambda);

  RealEigenVector lambda_eigen = Eigen::Map<RealEigenVector>(lambda.data(), lambda.size());

  return lambda_eigen;
}

RealEigenVector
StressResponseBase::computeT2(RealEigenVector lambda)
{
  TIME_SECTION("computeT2", 4, "Computing T2");
  // final sensitivity
  std::vector<Real> T2(_n_el);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    RealEigenVector lambda_el = lambda(_elem_to_dof_map[id]);
    RealEigenVector u_el = _U(_elem_to_dof_map[id]);
    Real value = lambda_el.transpose() * _KE * elem_data.u_el;
    T2[id] = -_p * std::pow(elem_data.physical_density, _p - 1) * (_E0 - _Emin) * value;
  }
  _communicator.sum(T2);
  RealEigenVector T2_eigen = Eigen::Map<RealEigenVector>(T2.data(), T2.size());
  return T2_eigen;
}

RealEigenMatrix
StressResponseBase::computeBMat(Real xi, Real eta)
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

RealEigenMatrix
StressResponseBase::getStress()
{
  return _stress;
}
