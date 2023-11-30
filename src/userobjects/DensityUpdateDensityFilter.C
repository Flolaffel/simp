//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "DensityUpdateDensityFilter.h"
#include <algorithm>

registerMooseObject("OptimizationApp", DensityUpdateDensityFilter);

InputParameters
DensityUpdateDensityFilter::validParams()
{
  InputParameters params = ElementUserObject::validParams();
  params.addClassDescription(
      "Compute updated densities based on sensitivities using an optimality criteria method to "
      "keep the volume constraint satisified.");
  params.addRequiredCoupledVar("design_density", "Design density variable name.");
  params.addRequiredParam<VariableName>("compliance_sensitivity",
                                        "Name of the compliance_sensitivity variable.");
  params.addRequiredParam<MeshGeneratorName>(
      "mesh_generator",
      "Name of the mesh generator to be used to retrieve control drums information.");
  params.addRequiredParam<Real>("volume_fraction", "Volume Fraction");
  params.addRequiredParam<Real>("radius", "Cut-off radius for the averaging");
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

DensityUpdateDensityFilter::DensityUpdateDensityFilter(const InputParameters & parameters)
  : ElementUserObject(parameters),
    _mesh(_subproblem.mesh()),
    _mesh_generator(getParam<MeshGeneratorName>("mesh_generator")),
    _compliance_sensitivity_name(getParam<VariableName>("compliance_sensitivity")),
    _design_density(&writableVariable("design_density")),
    _compliance_sensitivity(&_subproblem.getStandardVariable(_tid, _compliance_sensitivity_name)),
    _volume_fraction(getParam<Real>("volume_fraction")),
    _radius(getParam<Real>("radius")),
    _nx(getMeshProperty<unsigned int>("num_elements_x", _mesh_generator)),
    _ny(getMeshProperty<unsigned int>("num_elements_y", _mesh_generator)),
    _xmin(getMeshProperty<Real>("xmin", _mesh_generator)),
    _xmax(getMeshProperty<Real>("xmax", _mesh_generator)),
    _ymin(getMeshProperty<Real>("ymin", _mesh_generator)),
    _ymax(getMeshProperty<Real>("ymax", _mesh_generator)),
    _lower_bound(getParam<Real>("bisection_lower_bound")),
    _upper_bound(getParam<Real>("bisection_upper_bound"))
{
  if (!dynamic_cast<MooseVariableFE<Real> *>(_design_density))
    paramError("design_density", "Design density must be a finite element variable");
}

void
DensityUpdateDensityFilter::initialize()
{
  gatherElementData();
  prepareFilter();
  performOptimCritLoop();
}

void
DensityUpdateDensityFilter::execute()
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
DensityUpdateDensityFilter::gatherElementData()
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
          elem->volume(),
          0);
      _elem_data_map[elem_id] = data;
      _total_allowable_volume += elem->volume();
    }

  _communicator.sum(_total_allowable_volume);
  _total_allowable_volume *= _volume_fraction;
}

void
DensityUpdateDensityFilter::prepareFilter()
{
  // Only eligibale for elemente size of 1 mm
  int upp_r = ceil(_radius);
  int size = _nx * _ny * std::pow((2 * (upp_r - 1) + 1), 2);
  std::vector<int> iH(size, 1);
  std::vector<int> jH(size, 1);
  std::vector<Real> sH(size, 0);
  int counter = 0;
  for (unsigned int i = 0; i < _ny; i++)
  {
    for (unsigned int j = 0; j < _nx; j++)
    {
      int e1 = i * _nx + j;
      for (int k = std::max<int>(i - (upp_r - 1), 0); k < std::min<int>(i + upp_r, _ny); k++)
      {
        for (int l = std::max<int>(j - (upp_r - 1), 0); l < std::min<int>(j + upp_r, _nx); l++)
        {
          int e2 = k * _nx + l;
          iH[counter] = e1;
          jH[counter] = e2;
          sH[counter] = std::max<double>(0,
                                         _radius - std::sqrt(std::pow(std::abs<int>(i - k), 2) +
                                                             std::pow(std::abs<int>(j - l), 2)));
          counter++;
        }
      }
    }
  }

  // Fill _H and with values sH at locations iH,jH
  _H.resize(_nx * _ny, std::vector<Real>(_nx * _ny, 0));
  _Hs.resize(_nx * _ny);
  for (int i = 0; i < counter; i++)
  {
    _H[iH[i]][jH[i]] = sH[i];
  }

  // Fill _Hs with the column sums of _H
  for (unsigned int i = 0; i < _H.size(); i++)
  {
    Real column_sum = 0;
    for (unsigned int j = 0; j < _H[i].size(); j++)
    {
      column_sum += _H[i][j];
    }
    _Hs[i] = column_sum;
  }
}

void
DensityUpdateDensityFilter::performOptimCritLoop()
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
    std::vector<Real> x(_nx * _ny);
    // Initialize the current total volume
    Real curr_total_volume = 0;
    // Loop over all elements
    for (auto && [id, elem_data] : _elem_data_map)
    {
      // Compute the updated density for the current element
      Real new_density = computeUpdatedDensity(elem_data.old_density, elem_data.sensitivity, lmid);
      // Add density to vector
      x[id] = new_density;
    }

    // Filter the new densities
    for (auto && [id, elem_data] : _elem_data_map)
    {
      Real filt_density = 0;
      for (unsigned int j = 0; j < _nx * _ny; j++)
      {
        filt_density += _H[id][j] * x[j];
      }
      filt_density /= _Hs[id];
      // Update the current filtered density for the current element
      elem_data.new_density = filt_density;
      // Update the current total volume
      curr_total_volume += filt_density * elem_data.volume;
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
DensityUpdateDensityFilter::computeUpdatedDensity(Real current_density, Real dc, Real lmid)
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
