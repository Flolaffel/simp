//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "StressResponseVerbartPMean.h"
#include "MooseVariableScalar.h"

registerMooseObject("OptimizationApp", StressResponseVerbartPMean);

InputParameters
StressResponseVerbartPMean::validParams()
{
  InputParameters params = StressResponseBase::validParams();
  params.addClassDescription("Computes the stress sensitivities for stress constrained topology "
                             "optimization using qp-relaxation and P-norm aggregation (2D ONLY).");
  return params;
}

StressResponseVerbartPMean::StressResponseVerbartPMean(const InputParameters & parameters)
  : StressResponseBase(parameters)
{
}

void
StressResponseVerbartPMean::computeStress()
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
    _stress.row(id) << vector.transpose();
    _micro_vonmises(id) = std::sqrt(std::pow(vector(0), 2) + std::pow(vector(1), 2) -
                                    vector(0) * vector(1) + 3 * std::pow(vector(2), 2));
  }
  _interpolated_micro_vonmises = _micro_vonmises;
}

void
StressResponseVerbartPMean::computeValue()
{
  TIME_SECTION("computeValue", 3, "Computing P-norm value");
  Real sum = 0;
  for (auto && [id, elem_data] : _elem_data_map)
  {
    if (_is_objective)
      mooseError("Verbart P-mean doesn't support objective.");
    if (_is_constraint)
      sum +=
          std::pow(elem_data.physical_density * (_micro_vonmises(id) / _scaled_limit - 1) + 1, _P);
  }
  _communicator.sum(sum);
  _value = std::pow(1.0 / _n_el * sum, 1.0 / _P) - 1;
  _scalar_value->reinit();
  _scalar_value->setValues(_value);
  _scalar_value->insert(_scalar_value->sys().solution());
  _scalar_value->sys().solution().close();
}

void
StressResponseVerbartPMean::computeSensitivity()
{
  TIME_SECTION("computeSensitivity", 3, "Computing Stress Sensitivity");
  /// dsigma^PM/drho and dsigma^PM/dsigma_VMi
  std::vector<Real> dPMdx_vec(_n_el), dPMdVM_vec(_n_el);
  Real sum = 0;
  for (auto && [id, elem_data] : _elem_data_map)
  {
    sum += std::pow(elem_data.physical_density * (_micro_vonmises(id) / _scaled_limit - 1) + 1, _P);
  }
  _communicator.sum(sum);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    dPMdx_vec[id] =
        std::pow(1.0 / _n_el, 1.0 / _P) * 1.0 / _P * std::pow(sum, 1.0 / _P - 1) * _P *
        std::pow(elem_data.physical_density * (_micro_vonmises(id) / _scaled_limit - 1) + 1,
                 _P - 1) *
        (_micro_vonmises(id) / _scaled_limit - 1);
    dPMdVM_vec[id] =
        std::pow(1.0 / _n_el, 1.0 / _P) * 1.0 / _P * std::pow(sum, 1.0 / _P - 1) * _P *
        std::pow(elem_data.physical_density * (_micro_vonmises(id) / _scaled_limit - 1) + 1,
                 _P - 1) *
        elem_data.physical_density / _scaled_limit;
  }
  _communicator.sum(dPMdx_vec);
  _communicator.sum(dPMdVM_vec);
  RealEigenVector dPMdx = Eigen::Map<RealEigenVector>(dPMdx_vec.data(), dPMdx_vec.size());
  RealEigenVector dPMdVM = Eigen::Map<RealEigenVector>(dPMdVM_vec.data(), dPMdVM_vec.size());

  /// dsigma_VMi/dsigma_i
  std::vector<RealEigenVector> dVMdS(_n_el, RealEigenVector(3));
  for (auto && [id, elem_data] : _elem_data_map)
  {
    dVMdS[id](0) = 0.5 / _micro_vonmises(id) * (2 * _stress(id, 0) - _stress(id, 1));
    dVMdS[id](1) = 0.5 / _micro_vonmises(id) * (2 * _stress(id, 1) - _stress(id, 0));
    dVMdS[id](2) = 3 / _micro_vonmises(id) * _stress(id, 2);
  }

  /// vector gamma
  std::vector<Real> gamma(_n_dofs);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    RealEigenVector vector = computeBMat(0, 0) * _E * dVMdS[id];
    int x = 0;
    for (auto & dof : _elem_to_dof_map[id])
    {
      gamma[dof] += dPMdVM(id) * vector(x);
      x++;
    }
  }
  _communicator.sum(gamma);

  /// vector lambda
  RealEigenVector lambda;
  lambda = computeLambda(gamma);

  /// final sensitivity
  RealEigenVector T1 = dPMdx;
  RealEigenVector T2 = computeT2(lambda);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    elem_data.stress_sensitivity = T1(id) + T2(id);
  }
}
