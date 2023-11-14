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
  params.addClassDescription("Computes compliance sensitivity needed for SIMP method analytically.");
  params.addRequiredCoupledVar("design_density", "Design density variable name.");
  params.addRequiredParam<MaterialPropertyName>("youngs_modulus",
                                                "DerivativeParsedMaterial for Youngs modulus.");
  params.addParam<MaterialPropertyName>("E0", "E0",
                                        "Real youngs modulus");
  params.addParam<MaterialPropertyName>("Emin", "Emin",
                                        "Minimum youngs modulus");
  params.addParam<MaterialPropertyName>("p", "p",
                                        "SIMP exponent");

  return params;
}

AnalyticComplianceSensitivity::AnalyticComplianceSensitivity(const InputParameters & parameters)
  : StrainEnergyDensity(parameters),
    _sensitivity(declareProperty<Real>(_base_name + "sensitivity")),
    _design_density(coupledValue("design_density")),
    _design_density_name(coupledName("design_density", 0)),
    _dEdp(getMaterialPropertyDerivativeByName<Real>(
        getParam<MaterialPropertyName>("youngs_modulus"), _design_density_name)),
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
  // dcdp = -power * p^(power-1) * (E0-Emin) * ue^T * k0 * ue
  _sensitivity[_qp] = -_power[_qp] * std::pow(_design_density[_qp],_power[_qp]-1) * (_E0[_qp]-_Emin[_qp]) * _strain_energy_density[_qp] / _youngs_modulus[_qp];
}
