//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "StressResponseAverageSolidStress.h"
#include "MooseVariableScalar.h"

registerMooseObject("OptimizationApp", StressResponseAverageSolidStress);

InputParameters
StressResponseAverageSolidStress::validParams()
{
  InputParameters params = StressResponseQpPNorm::validParams();
  params.addClassDescription("Computes the stress sensitivities for stress constrained topology "
                             "optimization using qp-relaxation and P-norm aggregation (2D ONLY).");
  params.addRequiredParam<UserObjectName>("stress_response",
                                          "Name of the stress response object to use");
  params.makeParamNotRequired("limit");
  params.set<Real>("limit") = 0;
  return params;
}

StressResponseAverageSolidStress::StressResponseAverageSolidStress(
    const InputParameters & parameters)
  : StressResponseQpPNorm(parameters),
    _stress_response(getUserObject<StressResponseBase>("stress_response"))
{
}

void
StressResponseAverageSolidStress::computeValue()
{
  TIME_SECTION("computeValue", 3, "Computing P-norm value");
  Real xPhys_sum = 0;
  for (auto && [id, elem_data] : _elem_data_map)
  {
    if (_is_objective)
      mooseError("Can't use solid stress as objective.");
    if (_is_constraint)
      xPhys_sum += elem_data.physical_density;
  }
  _communicator.sum(xPhys_sum);
  Real test = _interpolated_micro_vonmises.sum();

  if (_t_step == 1)
    _limit = _interpolated_micro_vonmises.maxCoeff();
  else if (_t_step % 10 == 0)
    _limit = _interpolated_micro_vonmises_old.maxCoeff();

  Real average_solid_stress = _interpolated_micro_vonmises.sum() / xPhys_sum;
  _value = average_solid_stress / _limit - 1;
  _scalar_value->reinit();
  _scalar_value->setValues(_value);
  _scalar_value->insert(_scalar_value->sys().solution());
  _scalar_value->sys().solution().close();
}

void
StressResponseAverageSolidStress::computeSensitivity()
{
  TIME_SECTION("computeSensitivity", 3, "Computing Stress Sensitivity");
  /// dsolidStress/dx
  Real xPhys_sum = 0;
  for (auto && [id, elem_data] : _elem_data_map)
    xPhys_sum += elem_data.physical_density;
  _communicator.sum(xPhys_sum);
  Real quotient = _interpolated_micro_vonmises.sum() / std::pow(xPhys_sum * _limit, 2);
  RealEigenVector dsolidSdx(_n_el);
  dsolidSdx.setOnes();
  dsolidSdx *= -quotient;

  /// dsolidStress/dsigma_VMi
  RealEigenVector dsolidSdVM(_n_el);
  dsolidSdVM.setOnes();
  dsolidSdVM *= 1 / (xPhys_sum * _limit);

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
    RealEigenVector u_el = _U(_elem_to_dof_map[id]);
    Real value = dVMdS[id].transpose() * _E * B.transpose() * u_el;
    beta_vec[id] = _q * std::pow(elem_data.physical_density, _q - 1) * value;
  }
  _communicator.sum(beta_vec);
  RealEigenVector beta = Eigen::Map<RealEigenVector>(beta_vec.data(), beta_vec.size());

  /// vector gamma
  std::vector<Real> gamma(_n_dofs);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    RealEigenVector vector = computeBMat(0, 0) * _E * dVMdS[id];
    int x = 0;
    for (auto & dof : _elem_to_dof_map[id])
    {
      gamma[dof] += dsolidSdVM(id) * std::pow(elem_data.physical_density, _q) * vector(x);
      x++;
    }
  }
  _communicator.sum(gamma);

  /// vector lambda
  RealEigenVector lambda = computeLambda(gamma);

  /// final sensitivity
  RealEigenVector T1 = dsolidSdx + dsolidSdVM.cwiseProduct(beta);
  RealEigenVector T2 = computeT2(lambda);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    elem_data.stress_sensitivity = T1(id) + T2(id);
  }
}
