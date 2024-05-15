//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "DensityUpdateOC.h"
#include "vector.h"
#include <algorithm>
#include "MooseVariableScalar.h"

registerMooseObject("OptimizationApp", DensityUpdateOC);

InputParameters
DensityUpdateOC::validParams()
{
  InputParameters params = FilterBase::validParams();
  params.addClassDescription("Compute updated densities based on sensitivities using OC with built "
                             "in density filter and heaviside projection options");
  params.addRequiredCoupledVar("design_density", "Design density variable name.");
  params.addCoupledVar("filtered_density",
                       "Filtered density variable name. Only needed for Heaviside.");
  params.addRequiredCoupledVar("physical_density", "Physical density variable name.");
  params.addRequiredParam<VariableName>("objective_function_sensitivity",
                                        "Name of the objective function sensitivity variable.");
  params.addParam<Real>("volume_fraction", "Volume Fraction");
  params.addParam<VariableName>("volume_sensitivity", "Volume sensitivity variable name.");
  params.addParam<Real>("bisection_lower_bound", 0, "Lower bound for the bisection algorithm.");
  params.addParam<Real>("bisection_upper_bound", 1e9, "Upper bound for the bisection algorithm.");
  params.addParam<Real>("move_limit", 0.2, "Move limit.");
  params.set<int>("execution_order_group") = 2;
  return params;
}

DensityUpdateOC::DensityUpdateOC(const InputParameters & parameters)
  : FilterBase(parameters),
    _design_density(&writableVariable("design_density")),
    _physical_density(&writableVariable("physical_density")),
    _objective_sensitivity_name(getParam<VariableName>("objective_function_sensitivity")),
    _objective_sensitivity(&_subproblem.getStandardVariable(_tid, _objective_sensitivity_name)),
    _volume_sensitivity_name(getParam<VariableName>("volume_sensitivity")),
    _volume_sensitivity(&_subproblem.getStandardVariable(_tid, _volume_sensitivity_name)),
    _volume_fraction(getParam<Real>("volume_fraction")),
    _move_limit(getParam<Real>("move_limit")),
    _lower_bound(getParam<Real>("bisection_lower_bound")),
    _upper_bound(getParam<Real>("bisection_upper_bound"))
{
  if (!dynamic_cast<MooseVariableFE<Real> *>(_design_density))
    paramError("design_density", "Design density must be a finite element variable");

  if (_filter_type == FilterType::SENSITIVITY)
  {
    mooseError("Option sensitivity filtering not allowed in OC density update.");
  }

  if (_filter_type == FilterType::HEAVISIDE)
  {
    _filtered_density = &writableVariable("filtered_density");
    _beta_0 = getParam<Real>("beta_0");
    _eta = getParam<Real>("eta");
  }
}

void
DensityUpdateOC::initialize()
{
  gatherElementData();
  performOcLoop();
}

void
DensityUpdateOC::execute()
{
  // Grab the element data for each id
  auto elem_data_iter = _elem_data_map.find(_current_elem->id());

  // Check if the element data is not null
  mooseAssert(elem_data_iter != _elem_data_map.end(),
              "Element data not found for the current element id.");

  ElementData & elem_data = elem_data_iter->second;
  dynamic_cast<MooseVariableFE<Real> *>(_design_density)
      ->setNodalValue(elem_data.new_design_density);
  if (_filter_type == FilterType::NONE)
    dynamic_cast<MooseVariableFE<Real> *>(_physical_density)
        ->setNodalValue(elem_data.new_design_density);
  if (_filter_type == FilterType::DENSITY)
    dynamic_cast<MooseVariableFE<Real> *>(_physical_density)
        ->setNodalValue(elem_data.new_filt_density);
  if (_filter_type == FilterType::HEAVISIDE)
  {
    dynamic_cast<MooseVariableFE<Real> *>(_filtered_density)
        ->setNodalValue(elem_data.new_filt_density);
    dynamic_cast<MooseVariableFE<Real> *>(_physical_density)
        ->setNodalValue(elem_data.new_proj_density);
  }
}

void
DensityUpdateOC::gatherElementData()
{
  _elem_data_map.clear();
  _total_allowable_volume = 0;

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();

      ElementData data;
      data = ElementData(
          dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem),
          dynamic_cast<const MooseVariableFE<Real> *>(_objective_sensitivity)
              ->getElementalValue(elem),
          dynamic_cast<const MooseVariableFE<Real> *>(_volume_sensitivity)->getElementalValue(elem),
          elem->volume(),
          0,
          0,
          0);
      _elem_data_map[elem_id] = data;
      _total_allowable_volume += elem->volume();
    }
  _communicator.sum(_total_allowable_volume);
  _total_allowable_volume *= _volume_fraction;
}

void
DensityUpdateOC::performOcLoop()
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
    std::vector<Real> new_density(_n_el);
    // Loop over all elements
    for (auto && [id, elem_data] : _elem_data_map)
    {
      // Compute the updated density for the current element
      new_density[id] = computeUpdatedDensity(elem_data.current_design_density,
                                              elem_data.objective_sensitivity,
                                              elem_data.volume_sensitivity,
                                              lmid);
    }
    std::vector<Real> phys_density = new_density;
    std::vector<Real> filt_density(_n_el), proj_density(_n_el);
    if (_filter_type == FilterType::DENSITY)
    {
      filt_density = densityFilter(new_density);
      phys_density = filt_density;
    }
    else if (_filter_type == FilterType::HEAVISIDE)
    {
      filt_density = densityFilter(new_density);
      proj_density = heavisideProjection(filt_density);
      phys_density = proj_density;
    }
    // Initialize the current total volume
    Real curr_total_volume = 0;
    // Assign new values
    for (auto && [id, elem_data] : _elem_data_map)
    {
      // Update the element data
      elem_data.new_design_density = new_density[id];
      elem_data.new_filt_density = filt_density[id];
      elem_data.new_proj_density = proj_density[id];
      curr_total_volume += phys_density[id] * elem_data.volume;
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
DensityUpdateOC::computeUpdatedDensity(Real current_density, Real dc, Real dv, Real lmid)
{
  // Define the maximum allowable change in density
  Real move = _move_limit;
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

std::vector<Real>
DensityUpdateOC::densityFilter(std::vector<Real> density)
{
  RealEigenVector dens_temp = Eigen::Map<RealEigenVector>(density.data(), density.size());
  RealEigenVector filt_temp = (_H * dens_temp).array() / _Hs.array();
  std::vector<Real> filt_density(filt_temp.data(), filt_temp.data() + filt_temp.size());
  return filt_density;
}

std::vector<Real>
DensityUpdateOC::heavisideProjection(std::vector<Real> filtered_density)
{
  Real beta = _beta_0;
  if (_t_step == 1)
    beta = _beta_0;
  else if (_t_step % 20 == 0)
  {
  }

  std::vector<Real> projected_density(_n_el);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    projected_density[id] =
        (std::tanh(beta * _eta) + std::tanh(beta * (filtered_density[id] - _eta))) /
        (std::tanh(beta * _eta) + std::tanh(beta * (1 - _eta)));
  }
  return projected_density;
}
