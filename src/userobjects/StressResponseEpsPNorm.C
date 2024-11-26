//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "StressResponseEpsPNorm.h"
#include "MooseVariableScalar.h"

registerMooseObject("OptimizationApp", StressResponseEpsPNorm);

InputParameters
StressResponseEpsPNorm::validParams()
{
  InputParameters params = StressResponseBase::validParams();
  params.addClassDescription("Computes the stress sensitivities for stress constrained topology "
                             "optimization using qp-relaxation and P-norm aggregation (2D ONLY).");
  params.addParam<Real>("epsilon", 0.2, "Stress interpolation parameter");
  return params;
}

StressResponseEpsPNorm::StressResponseEpsPNorm(const InputParameters & parameters)
  : StressResponseBase(parameters), _eps(getParam<Real>("epsilon"))
{
}

void
StressResponseEpsPNorm::computeStress()
{
  TIME_SECTION("computeStress", 3, "Computing stress at element center");

  _interpolated_micro_vonmises_old = _interpolated_micro_vonmises;

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
    RealEigenVector interpolated_vector =
        elem_data.physical_density /
        (_eps * (1 - elem_data.physical_density) + elem_data.physical_density) * vector;
    _stress.row(id) << interpolated_vector.transpose();
    _interpolated_micro_vonmises(id) = std::sqrt(
        std::pow(interpolated_vector(0), 2) + std::pow(interpolated_vector(1), 2) -
        interpolated_vector(0) * interpolated_vector(1) + 3 * std::pow(interpolated_vector(2), 2));
    _micro_vonmises(id) = std::sqrt(std::pow(vector(0), 2) + std::pow(vector(1), 2) -
                                    vector(0) * vector(1) + 3 * std::pow(vector(2), 2));
  }
}

void
StressResponseEpsPNorm::computeValue()
{
  TIME_SECTION("computeValue", 3, "Computing P-norm value");
  Real sum = 0;
  for (auto && [id, elem_data] : _elem_data_map)
  {
    sum += std::pow(_interpolated_micro_vonmises(id), _P);
  }
  _communicator.sum(sum);
  Real PN = std::pow(sum, 1.0 / _P);

  if (_scaling)
  {
    _console << "old limit = " << _scaled_limit;
    _scaled_limit = std::max(_limit, _limit * PN / _interpolated_micro_vonmises.maxCoeff());
    _console << ". new limit = " << _scaled_limit << std::endl;
  }
  else if (!_scaling)
    _scaled_limit = _limit;

  if (_is_objective)
    _value = PN;
  if (_is_constraint)
    _value = PN / _scaled_limit - 1;
  _scalar_value->setValues(_value);
  _scalar_value->insert(_scalar_value->sys().solution());
  _scalar_value->sys().solution().close();
}

void
StressResponseEpsPNorm::computeSensitivity()
{
  TIME_SECTION("computeSensitivity", 3, "Computing Stress Sensitivity");
  /// dsigma^PN/dsigma_VMi
  std::vector<Real> dPNdVM_vec(_n_el);
  Real sum = 0;
  for (auto && [id, elem_data] : _elem_data_map)
  {
    sum += std::pow(_interpolated_micro_vonmises(id) / _limit, _P);
  }
  _communicator.sum(sum);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    dPNdVM_vec[id] = 1.0 / _P * std::pow(sum, 1.0 / _P - 1) * _P *
                     std::pow(_interpolated_micro_vonmises(id) / _limit, _P - 1) * 1 / _limit;
  }
  _communicator.sum(dPNdVM_vec);
  RealEigenVector dPNdVM = Eigen::Map<RealEigenVector>(dPNdVM_vec.data(), dPNdVM_vec.size());

  /// dsigma_VMi/dsigma_i
  std::vector<RealEigenVector> dVMdS(_n_el, RealEigenVector(3));
  for (auto && [id, elem_data] : _elem_data_map)
  {
    dVMdS[id](0) = 0.5 / _interpolated_micro_vonmises(id) * (2 * _stress(id, 0) - _stress(id, 1));
    dVMdS[id](1) = 0.5 / _interpolated_micro_vonmises(id) * (2 * _stress(id, 1) - _stress(id, 0));
    dVMdS[id](2) = 3 / _interpolated_micro_vonmises(id) * _stress(id, 2);
  }

  /// vector beta
  std::vector<Real> beta_vec(_n_el);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    RealEigenMatrix B = computeBMat(0, 0);
    // RealEigenVector u_el = _U(_elem_to_dof_map_test[id].second);
    Real value = dVMdS[id].transpose() * _E * B.transpose() * elem_data.u_el;
    beta_vec[id] = _eps / std::pow(elem_data.physical_density * (_eps - 1) - _eps, 2) * value;
  }
  _communicator.sum(beta_vec);
  RealEigenVector beta = Eigen::Map<RealEigenVector>(beta_vec.data(), beta_vec.size());

  /// vector gamma
  std::vector<Real> gamma(_n_dofs);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    RealEigenVector vector = computeBMat(0, 0) * _E * dVMdS[id];
    int x = 0;
    // for (auto & dof : _elem_to_dof_map_test[id].second)
    for (auto & dof : _elem_to_dof_map[id])
    {
      gamma[dof] += dPNdVM(id) * elem_data.physical_density /
                    (_eps * (1 - elem_data.physical_density) + elem_data.physical_density) *
                    vector(x);
      x++;
    }
  }
  _communicator.sum(gamma);

  /// vector lambda
  RealEigenVector lambda;
  lambda = computeLambda(gamma);

  /// final sensitivity
  RealEigenVector T1 = dPNdVM.cwiseProduct(beta);
  RealEigenVector T2 = computeT2(lambda);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    elem_data.stress_sensitivity = T1(id) + T2(id);
  }
}
