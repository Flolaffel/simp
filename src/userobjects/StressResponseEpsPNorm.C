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
  _stress.resize(_n_el, 3);
  _vonmises.resize(_n_el);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    RealEigenVector vector =
        elem_data.physical_density /
        (_eps * (1 - elem_data.physical_density) + elem_data.physical_density) * _E *
        getBMat(0, 0).transpose() * elem_data.u_el;
    _stress.row(id) << vector.transpose();
    _vonmises(id) = std::sqrt(std::pow(vector(0), 2) + std::pow(vector(1), 2) -
                              vector(0) * vector(1) + 3 * std::pow(vector(2), 2));
  }
}

void
StressResponseEpsPNorm::computeValue()
{
  Real PN = 0;
  for (auto && [id, elem_data] : _elem_data_map)
  {
    if (_is_objective)
      PN += std::pow(_vonmises(id), _P);
    if (_is_constraint)
      PN += std::pow(_vonmises(id) / _limit, _P);
  }
  PN = std::pow(PN, 1.0 / _P) - 1;
  _value->reinit();
  _value->setValues(PN);
  _value->insert(_value->sys().solution());
  _value->sys().solution().close();
}

void
StressResponseEpsPNorm::computeSensitivity()
{
  /// dsigma^PN/dsigma_VMi
  RealEigenVector dPNdVM(_n_el);
  Real sum = 0;
  for (auto && [id, elem_data] : _elem_data_map)
  {
    sum += std::pow(_vonmises(id) / _limit, _P);
  }
  for (auto && [id, elem_data] : _elem_data_map)
  {
    dPNdVM(id) = 1.0 / _P * std::pow(sum, 1.0 / _P - 1) * _P *
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
  RealEigenVector beta(_n_el);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    RealEigenMatrix B = getBMat(0, 0);
    Real value = dVMdS[id].transpose() * _E * B.transpose() * _elem_data_map[id].u_el;
    beta(id) = _eps / std::pow(elem_data.physical_density * (_eps - 1) - _eps, 2) * value;
  }

  /// vector gamma and gamma_red
  std::vector<Real> gamma(_n_dofs), gamma_red(_n_dofs);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    RealEigenVector vector = getBMat(0, 0) * _E * dVMdS[id];
    int x = 0;
    for (auto & dof : _elem_to_dof_map[id])
    {
      gamma[dof] += dPNdVM(id) * elem_data.physical_density /
                    (_eps * (1 - elem_data.physical_density) + elem_data.physical_density) *
                    vector(x);
      bool dof_is_free =
          std::find(std::begin(_free_dofs), std::end(_free_dofs), dof) != std::end(_free_dofs);
      if (dof_is_free)
        gamma_red[dof] += dPNdVM(id) * elem_data.physical_density /
                          (_eps * (1 - elem_data.physical_density) + elem_data.physical_density) *
                          vector(x);
      x++;
    }
  }

  /// vector lambda
  RealEigenVector lambda;
  lambda = getLambda(gamma_red);

  /// final sensitivity
  RealEigenVector T1 = dPNdVM.cwiseProduct(beta);
  RealEigenVector T2 = getT2(lambda);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    elem_data.stress_sensitivity = T1(id) + T2(id);
  }
}