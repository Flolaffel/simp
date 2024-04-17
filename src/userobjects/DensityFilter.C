//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "DensityFilter.h"
#include "MooseError.h"
#include <algorithm>

registerMooseObject("OptimizationApp", DensityFilter);

InputParameters
DensityFilter::validParams()
{
  InputParameters params = Filter::validParams();
  params.addClassDescription("Beta projection for SIMP topology optimization");
  params.addRequiredCoupledVar("design_density", "Design density variable name.");
  params.addRequiredCoupledVar("physical_density", "Physical density variable name.");
  params.set<bool>("force_postaux") = true;
  params.set<int>("execution_order_group") = 3;
  return params;
}

DensityFilter::DensityFilter(const InputParameters & parameters)
  : Filter(parameters),
    _design_density(&writableVariable("design_density")),
    _physical_density(&writableVariable("physical_density"))
{
}

void
DensityFilter::initialize()
{
  gatherElementData();
  densityFilter();
}

void
DensityFilter::execute()
{
  // Grab the element data for each id
  auto elem_data_iter = _elem_data_map.find(_current_elem->id());

  // Check if the element data is not null
  if (elem_data_iter != _elem_data_map.end())
  {
    ElementData & elem_data = elem_data_iter->second;
    dynamic_cast<MooseVariableFE<Real> *>(_physical_density)
        ->setNodalValue(elem_data.filtered_density);
  }
  else
  {
    mooseError("Element data not found for the current element id.");
  }
}

void
DensityFilter::threadJoin(const UserObject & y)
{
  const DensityFilter & uo = static_cast<const DensityFilter &>(y);
  _elem_data_map.insert(uo._elem_data_map.begin(), uo._elem_data_map.end());
}

void
DensityFilter::gatherElementData()
{
  _elem_data_map.clear();

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();

      ElementData data = ElementData(
          dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem), 0);
      _elem_data_map[elem_id] = data;
    }
}

void
DensityFilter::densityFilter()
{
  for (auto && [id, elem_data] : _elem_data_map)
  {
    for (unsigned int j = 0; j < _n_el; j++)
    {
      elem_data.filtered_density += _H[id][j] * _elem_data_map[j].design_density;
    }
    elem_data.filtered_density /= _Hs[id];
  }
}
