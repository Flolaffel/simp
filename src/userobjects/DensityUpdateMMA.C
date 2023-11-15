//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "DensityUpdateMMA.h"
#include <algorithm>
#include "Output.h"

registerMooseObject("OptimizationApp", DensityUpdateMMA);

InputParameters
DensityUpdateMMA::validParams()
{
  InputParameters params = ElementUserObject::validParams();
  params.addClassDescription(
      "Compute updated densities based on sensitivities using an optimality criteria method to "
      "keep the volume constraint satisified.");
  params.addRequiredCoupledVar("design_density", "Design density variable name.");
  params.addRequiredCoupledVar("old_design_density", "Old design density variable name.");
  params.addRequiredParam<VariableName>("density_sensitivity",
                                        "Name of the density_sensitivity variable.");
  params.addRequiredParam<Real>("volume_fraction", "Volume Fraction");
  params.addParam<Real>("bisection_lower_bound", 0, "Lower bound for the bisection algorithm.");
  params.addParam<Real>("bisection_upper_bound", 1e16, "Upper bound for the bisection algorithm.");
  params.addParam<int>(
      "execution_order_group",
      1,
      "Execution order groups are executed in increasing order (e.g., the lowest "
      "number is executed first). Note that negative group numbers may be used to execute groups "
      "before the default (0) group. Please refer to the user object documentation "
      "for ordering of user object execution within a group.");
  return params;
}

DensityUpdateMMA::DensityUpdateMMA(const InputParameters & parameters)
  : ElementUserObject(parameters),
    _mesh(_subproblem.mesh()),
    _density_sensitivity_name(getParam<VariableName>("density_sensitivity")),
    _design_density(&writableVariable("design_density")),
    _old_design_density(&writableVariable("old_design_density")),
    _density_sensitivity(&_subproblem.getStandardVariable(_tid, _density_sensitivity_name)),
    _volume_fraction(getParam<Real>("volume_fraction")),
    _lower_bound(getParam<Real>("bisection_lower_bound")),
    _upper_bound(getParam<Real>("bisection_upper_bound"))
{
  if (!dynamic_cast<MooseVariableFE<Real> *>(_design_density))
    paramError("design_density", "Design density must be a finite element variable");
  if (!dynamic_cast<MooseVariableFE<Real> *>(_old_design_density))
    paramError("old_design_density", "Old design density must be a finite element variable");
}

void
DensityUpdateMMA::initialize()
{
  gatherElementData();
  performMMALoop();
}

void
DensityUpdateMMA::execute()
{
  // Grab the element data for each id
  auto elem_data_iter = _elem_data_map.find(_current_elem->id());

  // Check if the element data is not null
  if (elem_data_iter != _elem_data_map.end())
  {
    ElementData & elem_data = elem_data_iter->second;
    dynamic_cast<MooseVariableFE<Real> *>(_design_density)->setNodalValue(elem_data.new_density);
    dynamic_cast<MooseVariableFE<Real> *>(_old_design_density)
        ->setNodalValue(elem_data.old_density1);
  }
  else
  {
    mooseError("Element data not found for the current element id.");
  }
}

void
DensityUpdateMMA::gatherElementData()
{
  _elem_data_map.clear();
  _total_allowable_volume = 0;

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();
      ElementData data = ElementData(
          dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem),
          dynamic_cast<MooseVariableFE<Real> *>(_old_design_density)->getElementalValue(elem),
          dynamic_cast<const MooseVariableFE<Real> *>(_density_sensitivity)
              ->getElementalValue(elem),
          elem->volume(),
          0);
      _elem_data_map[elem_id] = data;
      _total_allowable_volume += elem->volume();
    }

  _communicator.sum(_total_allowable_volume);
  _total_allowable_volume *= _volume_fraction;
}

void
DensityUpdateMMA::performMMALoop()
{
  // Initialize the lower and upper bounds for the bisection method
  Real l1 = _lower_bound;
  Real l2 = _upper_bound;
  bool perform_loop = true;
  // Loop until the relative difference between l1 and l2 is less than a small tolerance
  while (perform_loop)
  {
    // Compute the midpoint between l1 and l2
    Real lmid = 0.5 * (l2 + l1);

    // Initialize the current total volume
    Real curr_total_volume = 0;
    // Loop over all elements
    for (auto && [id, elem_data] : _elem_data_map)
    {
      // Compute the updated density for the current element
      Real new_density = computeUpdatedDensity(elem_data.old_density1, elem_data.sensitivity, lmid);
      // Update the current filtered density for the current element
      elem_data.new_density = new_density;
      // Update the current total volume
      curr_total_volume += new_density * elem_data.volume;
    }

    // Sum the current total volume across all processors
    _communicator.sum(curr_total_volume);

    // Update l1 or l2 based on whether the current total volume is greater than the total
    // allowable volume
    if (curr_total_volume > _total_allowable_volume)
      l1 = lmid;
    else
      l2 = lmid;

    // Determine whether to continue the loop based on the relative difference between l1 and l2
    perform_loop = (l2 - l1) / (l1 + l2) > 1e-3;
  }
}

// Method to compute the updated density for an element
Real
DensityUpdateMMA::computeUpdatedDensity(Real current_density, Real dc, Real lmid)
{
  // Define the maximum allowable change in density
  Real move = 0.2;
  // Compute the updated density based on the current density, the sensitivity, and the midpoint
  // value
  Real updated_density =
      std::max(0.0,
               std::max(current_density - move,
                        std::min(1.0,
                                 std::min(current_density + move,
                                          current_density * std::sqrt(-dc / lmid)))));
  // Return the updated density
  return updated_density;
}

/*
void
DensityUpdateMMA::execute()
{
  // Grab the element data for each id
  auto elem_data_iter = _elem_data_map.find(_current_elem->id());

  // Check if the element data is not null
  if (elem_data_iter != _elem_data_map.end())
  {
    ElementData & elem_data = elem_data_iter->second;
    dynamic_cast<MooseVariableFE<Real> *>(_design_density)->setNodalValue(elem_data.new_density);
  }
  else
  {
    mooseError("Element data not found for the current element id.");
  }
}

void
DensityUpdateMMA::gatherElementData()
{
  _elem_data_map.clear();
  _total_allowable_volume = 0;

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();
      ElementData data = ElementData(
          dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem),
          dynamic_cast<const MooseVariableFE<Real> *>(_density_sensitivity)
              ->getElementalValue(elem),
          elem->volume(),
          0);
      _elem_data_map[elem_id] = data;
      _total_allowable_volume += elem->volume();
    }

  _communicator.sum(_total_allowable_volume);
  _total_allowable_volume *= _volume_fraction;
}

void
DensityUpdateMMA::performMMALoop()
{
  // INIT

  // Number of constraints
  m = 1;
  // Number of design variables
  n = 1;
  // Column vector with the lower bounds for the variables x_j
  xmin = 0;
  // Column vector with the upper bounds for the variables x_j
  xmax = 1;
  // xval, one iteration ago (provided that iter>1)
  xold1 = elem_data.old_density1;
  // xval, two iterations ago (provided that iter>2)
  xold2 = elem_data.old_density2;
  // Column vector with the lower asymptotes from the previous iteration (provided that iter>1)
  low = 1;
  // Column vector with the upper asymptotes from the previous iteration (provided that iter>1)
  upp = 1;
  // The constants a_0 in the term a_0*z
  a0 = 1;
  // Column vector with the constants a_i in the terms a_i*z
  a = 0;
  // Column vector with the constants c_i in the terms c_i*y_i
  c_MMA = 10000;
  // Column vector with the constants d_i in the terms 0.5*d_i*(y_i)^2
  d = 0;

  // MMA
  xval = x( :);
  f0val = c;
  df0dx = dc( :);
  fval = sum(xPhys( :)) / (volfrac * nele) - 1;
  dfdx = dv(
             :)' / (volfrac*nele);
         /*[xmma, ~, ~, ~, ~, ~, ~, ~, ~, low,upp] = ...
         mmasub(m, n, loop, xval, xmin, xmax, xold1, xold2, ...
         f0val,df0dx,fval,dfdx,low,upp,a0,a,c_MMA,d);
         % Update MMA Variables xnew = reshape(xmma, nely, nelx, nelz);
  xPhys( :) = (H * xnew( :))./ Hs;
  xold2 = xold1( :);
  xold1 = x( :);
}

// Method to compute the updated density for an element
Real
DensityUpdateMMA::computeUpdatedDensity(Real current_density, Real dc, Real lmid)
{
  // Define the maximum allowable change in density
  Real move = 0.2;
  // Compute the updated density based on the current density, the sensitivity, and the midpoint
  // value
  Real updated_density =
      std::max(0.0,
               std::max(current_density - move,
                        std::min(1.0,
                                 std::min(current_density + move,
                                          current_density * std::sqrt(-dc / lmid)))));
  // Return the updated density
  return updated_density;
}
*/
