//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ComputeVariableIsotropicElasticityTensorPlaneStress.h"

#include <algorithm>

registerMooseObject("TensorMechanicsApp", ComputeVariableIsotropicElasticityTensorPlaneStress);

InputParameters
ComputeVariableIsotropicElasticityTensorPlaneStress::validParams()
{
  InputParameters params = ComputeElasticityTensorBase::validParams();
  params.addClassDescription(
      "Compute an isotropic plane stress elasticity tensor for elastic constants that "
                             "change as a function of material properties");
  params.addRequiredParam<MaterialPropertyName>(
      "youngs_modulus", "Name of material property defining the Young's Modulus");
  params.addRequiredParam<MaterialPropertyName>(
      "poissons_ratio", "Name of material property defining the Poisson's Ratio");
  params.addRequiredCoupledVar(
      "args", "Variable dependence for the Young's Modulus and Poisson's Ratio materials");
  return params;
}

ComputeVariableIsotropicElasticityTensorPlaneStress::
    ComputeVariableIsotropicElasticityTensorPlaneStress(const InputParameters & parameters)
  : ComputeElasticityTensorBase(parameters),
    _youngs_modulus(getMaterialProperty<Real>("youngs_modulus")),
    _poissons_ratio(getMaterialProperty<Real>("poissons_ratio")),
    _num_args(coupledComponents("args")),
    _dyoungs_modulus(_num_args),
    _d2youngs_modulus(_num_args),
    _dpoissons_ratio(_num_args),
    _d2poissons_ratio(_num_args),
    _delasticity_tensor(_num_args),
    _d2elasticity_tensor(_num_args),
    _isotropic_elastic_constants(2)
{
  // all tensors created by this class are always isotropic
  // issueGuarantee(_elasticity_tensor_name, Guarantee::ISOTROPIC);

  // fetch prerequisite derivatives and build elasticity tensor derivatives and cross-derivatives
  for (unsigned int i = 0; i < _num_args; ++i)
  {
    const VariableName & iname = coupledName("args", i);
    _dyoungs_modulus[i] = &getMaterialPropertyDerivative<Real>("youngs_modulus", iname);
    _dpoissons_ratio[i] = &getMaterialPropertyDerivative<Real>("poissons_ratio", iname);

    _delasticity_tensor[i] =
        &declarePropertyDerivative<RankFourTensor>(_elasticity_tensor_name, iname);

    _d2youngs_modulus[i].resize(_num_args);
    _d2poissons_ratio[i].resize(_num_args);
    _d2elasticity_tensor[i].resize(_num_args);

    for (unsigned int j = i; j < _num_args; ++j)
    {
      const VariableName & jname = coupledName("args", j);
      _d2youngs_modulus[i][j] =
          &getMaterialPropertyDerivative<Real>("youngs_modulus", iname, jname);
      _d2poissons_ratio[i][j] =
          &getMaterialPropertyDerivative<Real>("poissons_ratio", iname, jname);
      _d2elasticity_tensor[i][j] =
          &declarePropertyDerivative<RankFourTensor>(_elasticity_tensor_name, iname, jname);
    }
  }
}

void
ComputeVariableIsotropicElasticityTensorPlaneStress::initialSetup()
{
  validateCoupling<Real>("youngs_modulus");
  validateCoupling<Real>("poissons_ratio");
  for (unsigned int i = 0; i < _num_args; ++i)
  {
    const VariableName & iname = coupledName("args", i);

    if (!_fe_problem.isMatPropRequested(
            derivativePropertyNameFirst(_elasticity_tensor_name, iname)))
      _delasticity_tensor[i] = nullptr;

    for (unsigned int j = 0; j < _num_args; ++j)
    {
      const VariableName & jname = coupledName("args", j);
      if (!_fe_problem.isMatPropRequested(
              derivativePropertyNameSecond(_elasticity_tensor_name, iname, jname)))
        _d2elasticity_tensor[i][j] = nullptr;
    }
  }
}

void
ComputeVariableIsotropicElasticityTensorPlaneStress::initQpStatefulProperties()
{
}

void
ComputeVariableIsotropicElasticityTensorPlaneStress::computeQpElasticityTensor()
{
  const Real E = _youngs_modulus[_qp];
  const Real nu = _poissons_ratio[_qp];
  const Real prefactor = E / (1 - std::pow(nu, 2));
  std::vector<Real> C{1, nu, 0, 1, 0, 0, 0, 0, (1 - nu) / 2};
  std::transform(
      std::begin(C), std::end(C), std::begin(C), [&prefactor](auto & c) { return c * prefactor; });

  _elasticity_tensor[_qp].fillSymmetric9FromInputVector(C);

  // Define derivatives of the elasticity tensor
  for (unsigned int i = 0; i < _num_args; ++i)
  {
    if (_delasticity_tensor[i])
    {
      const Real dE = (*_dyoungs_modulus[i])[_qp];
      const Real dnu = (*_dpoissons_ratio[i])[_qp];

      const Real dprefactor = dE / (1 - std::pow(dnu, 2));
      std::vector<Real> dC{1, dnu, 0, 1, 0, 0, 0, 0, (1 - dnu) / 2};
      std::transform(std::begin(dC),
                     std::end(dC),
                     std::begin(dC),
                     [&dprefactor](auto & c) { return c * dprefactor; });

      (*_delasticity_tensor[i])[_qp].fillSymmetric9FromInputVector(dC);
    }

    for (unsigned int j = i; j < _num_args; ++j)
      if (_d2elasticity_tensor[i][j])
      {
        const Real dEi = (*_dyoungs_modulus[i])[_qp];
        const Real dnui = (*_dpoissons_ratio[i])[_qp];

        const Real dEj = (*_dyoungs_modulus[j])[_qp];
        const Real dnuj = (*_dpoissons_ratio[j])[_qp];

        const Real d2E = (*_d2youngs_modulus[i][j])[_qp];
        const Real d2nu = (*_d2poissons_ratio[i][j])[_qp];

        const Real d2prefactor = d2E / 1 - (std::pow(d2nu, 2));
        std::vector<Real> d2C{1, d2nu, 0, 1, 0, 0, 0, 0, (1 - d2nu) / 2};
        std::transform(std::begin(d2C),
                       std::end(d2C),
                       std::begin(d2C),
                       [&d2prefactor](auto & c) { return c * d2prefactor; });

        (*_d2elasticity_tensor[i][j])[_qp].fillSymmetric9FromInputVector(d2C);
      }
  }
}
