//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "AnalyticComplianceSensitivity.h"
#include "Output.h"

registerMooseObject("TensorMechanicsApp", AnalyticComplianceSensitivity);

InputParameters
AnalyticComplianceSensitivity::validParams()
{
  InputParameters params = StrainEnergyDensity::validParams();
  params.addClassDescription(
      "Analytically computes compliance sensitivity needed for SIMP method.");
  params.addRequiredCoupledVar("physical_density", "Phyiscal density variable name.");
  params.addRequiredParam<MaterialPropertyName>("youngs_modulus",
                                                "DerivativeParsedMaterial for Youngs modulus.");
  params.addParam<MaterialPropertyName>("E0", "E0", "Solid material youngs modulus");
  params.addParam<MaterialPropertyName>("Emin", "Emin", "Minimum youngs modulus");
  params.addParam<MaterialPropertyName>("p", "p", "SIMP exponent");

  return params;
}

AnalyticComplianceSensitivity::AnalyticComplianceSensitivity(const InputParameters & parameters)
  : StrainEnergyDensity(parameters),
    _sensitivity(declareProperty<Real>(_base_name + "sensitivity")),
    _physical_density(coupledValue("physical_density")),
    _physical_density_name(coupledName("physical_density", 0)),
    _youngs_modulus(getMaterialProperty<Real>(getParam<MaterialPropertyName>("youngs_modulus"))),
    _E0(getMaterialProperty<Real>(getParam<MaterialPropertyName>("E0"))),
    _Emin(getMaterialProperty<Real>(getParam<MaterialPropertyName>("Emin"))),
    _power(getMaterialProperty<Real>(getParam<MaterialPropertyName>("p")))
{
}

void
AnalyticComplianceSensitivity::computeQpProperties()
{
  // Call the parent class's method to compute the strain energy density
  StrainEnergyDensity::computeQpProperties();
  // dcdx = -power * x^(power-1) * (E0-Emin) * ue^T * k0 * ue
  // with W = 1/2 * U * K * U = 1/2 * Ee * ue^T * k0 * ue
  // --> ue^T * k0 * ue = 2 * W / E
  _sensitivity[_qp] = -_power[_qp] * std::pow(_physical_density[_qp], _power[_qp] - 1) *
                      (_E0[_qp] - _Emin[_qp]) * 2 * _strain_energy_density[_qp] /
                      _youngs_modulus[_qp];
}
