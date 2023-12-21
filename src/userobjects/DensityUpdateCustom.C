//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "DensityUpdateCustom.h"
#include <algorithm>

registerMooseObject("OptimizationApp", DensityUpdateCustom);

InputParameters
DensityUpdateCustom::validParams()
{
  InputParameters params = Filter::validParams();
  params.addClassDescription(
      "Compute updated densities based on sensitivities using an optimality criteria method to "
      "keep the volume constraint satisified.");
  params.addRequiredCoupledVar("design_density", "Design density variable name.");
  params.addRequiredParam<VariableName>("compliance_sensitivity",
                                        "Name of the compliance sensitivity variable.");
  params.addRequiredParam<VariableName>("volume_sensitivity",
                                        "Name of the volume sensitivity variable.");
  params.addRequiredParam<Real>("volume_fraction", "Volume Fraction");
  params.addParam<Real>("bisection_lower_bound", 0, "Lower bound for the bisection algorithm.");
  params.addParam<Real>("bisection_upper_bound", 1e9, "Upper bound for the bisection algorithm.");
  params.addParam<int>(
      "execution_order_group",
      1,
      "Execution order groups are executed in increasing order (e.g., the lowest "
      "number is executed first). Note that negative group numbers may be used to execute groups "
      "before the default (0) group. Please refer to the user object documentation "
      "for ordering of user object execution within a group.");
  params.addCoupledVar("physical_density", "Physical density variable name.");
  return params;
}

DensityUpdateCustom::DensityUpdateCustom(const InputParameters & parameters)
  : Filter(parameters),
    _design_density(&writableVariable("design_density")),
    _physical_density(&writableVariable("physical_density")),
    _compliance_sensitivity_name(getParam<VariableName>("compliance_sensitivity")),
    _compliance_sensitivity(&_subproblem.getStandardVariable(_tid, _compliance_sensitivity_name)),
    _volume_sensitivity_name(getParam<VariableName>("volume_sensitivity")),
    _volume_sensitivity(&_subproblem.getStandardVariable(_tid, _volume_sensitivity_name)),
    _volume_fraction(getParam<Real>("volume_fraction")),
    _lower_bound(getParam<Real>("bisection_lower_bound")),
    _upper_bound(getParam<Real>("bisection_upper_bound"))
{
  if (!dynamic_cast<MooseVariableFE<Real> *>(_design_density))
    paramError("design_density", "Design density must be a finite element variable");
  if (!dynamic_cast<MooseVariableFE<Real> *>(_physical_density))
    paramError("physical_density", "Physical density must be a finite element variable");
  if (_filter_type == FilterType::SENSITIVITY)
    paramError("filter_type", "Sensitivity filtering is not a viable option for density update");
}

void
DensityUpdateCustom::initialize()
{
  gatherElementData();
  if (_filter_type == FilterType::DENSITY)
    Filter::prepareFilter();
  performOptimCritLoop();
}

void
DensityUpdateCustom::execute()
{
  // Grab the element data for each id
  auto elem_data_iter = _elem_data_map.find(_current_elem->id());

  // Check if the element data is not null
  if (elem_data_iter != _elem_data_map.end())
  {
    ElementData & elem_data = elem_data_iter->second;
    dynamic_cast<MooseVariableFE<Real> *>(_design_density)
        ->setNodalValue(elem_data.new_design_density);
    dynamic_cast<MooseVariableFE<Real> *>(_physical_density)
        ->setNodalValue(elem_data.new_phys_density);
  }
  else
  {
    mooseError("Element data not found for the current element id.");
  }
}

void
DensityUpdateCustom::gatherElementData()
{
  _elem_data_map.clear();
  _total_allowable_volume = 0;

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();
      ElementData data = ElementData(
          dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem),
          dynamic_cast<const MooseVariableFE<Real> *>(_compliance_sensitivity)
              ->getElementalValue(elem),
          dynamic_cast<const MooseVariableFE<Real> *>(_volume_sensitivity)->getElementalValue(elem),
          elem->volume(),
          0,
          0);
      _elem_data_map[elem_id] = data;
      _total_allowable_volume += elem->volume();
    }

  _communicator.sum(_total_allowable_volume);
  _total_allowable_volume *= _volume_fraction;
}

void
DensityUpdateCustom::performOptimCritLoop()
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

    // Initialize a vector holding the new densities
    unsigned int n_el = _elem_data_map.size();
    std::vector<Real> x(n_el);
    // Initialize the current total volume
    Real curr_total_volume = 0;
    // Loop over all elements
    for (auto && [id, elem_data] : _elem_data_map)
    {
      // Compute the updated density for the current element
      Real new_density = computeUpdatedDensity(elem_data.current_density,
                                               elem_data.compliance_sensitivity,
                                               elem_data.volume_sensitivity,
                                               lmid);
      // Update design and physical density
      elem_data.new_design_density = new_density;
      elem_data.new_phys_density = new_density;
      // Add design density to vector
      if (_filter_type == FilterType::DENSITY)
        x[id] = new_density;
      else
        curr_total_volume += new_density * elem_data.volume;
    }

    // Filter the new densities
    if (_filter_type == FilterType::DENSITY)
    {
      for (auto && [id, elem_data] : _elem_data_map)
      {
        Real filt_density = 0;
        for (unsigned int j = 0; j < n_el; j++)
        {
          filt_density += _H[id][j] * x[j];
        }
        filt_density /= _Hs[id];
        // Update the physical (filtered) density
        elem_data.new_phys_density = filt_density;
        // Update the current total volume
        curr_total_volume += filt_density * elem_data.volume;
      }
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
DensityUpdateCustom::computeUpdatedDensity(Real current_density, Real dc, Real dv, Real lmid)
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
                                          current_density * std::sqrt(-dc / dv / lmid)))));
  // Return the updated density
  return updated_density;
}
