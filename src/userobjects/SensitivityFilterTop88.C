//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "SensitivityFilterTop88.h"
#include "MooseError.h"
#include "Output.h"
#include <algorithm>

registerMooseObject("OptimizationApp", SensitivityFilterTop88);

InputParameters
SensitivityFilterTop88::validParams()
{
  InputParameters params = ElementUserObject::validParams();
  params.addClassDescription(
      "Computes the filtered sensitivity using a radial average user object.");
  params.addRequiredParam<UserObjectName>("filter_UO", "Radial Average user object");
  params.addRequiredCoupledVar("density_sensitivity", "Name of the density_sensitivity variable.");
  params.addRequiredParam<VariableName>("design_density", "Design density variable name.");

  return params;
}

SensitivityFilterTop88::SensitivityFilterTop88(const InputParameters & parameters)
  : ElementUserObject(parameters),
    _filter(getUserObject<RadialAverage>("filter_UO").getAverage()),
    _density_sensitivity(writableVariable("density_sensitivity")),
    _design_density_name(getParam<VariableName>("design_density")),
    _design_density(_subproblem.getStandardVariable(_tid, _design_density_name))
{
}

void
SensitivityFilterTop88::execute()
{
  // Find the current element in the filter
  auto filter_iter = _filter.find(_current_elem->id());

  // Assert the element is found in the filter
  mooseAssert(filter_iter != _filter.end(),
              "An element could not be found in the filter. Check that a RadialAverage user object "
              "has run before this object.");

  // Get the quadrature point values from the filter
  std::vector<Real> qp_vals = filter_iter->second;

  // Initialize the total elemental sensitivity value
  Real den_sense_val = 0;
  
  // Get the pseudo density for the current element
  Real x = _design_density.getElementalValue(_current_elem);

  // Compute the total elemental sensitivity value by summing over all quadrature points
  for (unsigned int qp = 0; qp < _qrule->n_points(); qp++)
    den_sense_val += qp_vals[qp] * _JxW[qp];

  den_sense_val = (den_sense_val)/(_current_elem_volume);
  /*_console << "current el: " << _current_elem->id() << "\n" << std::flush;
  _console << "current vol: " << _current_elem_volume << "\n" << std::flush;
  _console << "current dens: " << _design_density.getElementalValue(_current_elem) << "\n" << std::flush;*/

  //_design_density.getElementalValue(

  // Set the nodal value of the density sensitivity
  _density_sensitivity.setNodalValue(den_sense_val);

  /*auto id = _current_elem->id();
  auto vol = _current_elem->volume();
  auto mid = _current_elem->vertex_average();

  std::ofstream outFile("top88.txt");
  for (Real i: qp_vals)
    outFile << i << "\n";

  _console << "ID: " << id  << ", vol: " << vol << ", mid: " << mid << "\n" << std::flush;*/
}
