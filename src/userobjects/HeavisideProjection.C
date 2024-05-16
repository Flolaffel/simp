//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "HeavisideProjection.h"
#include "MooseError.h"
#include <algorithm>

registerMooseObject("OptimizationApp", HeavisideProjection);

InputParameters
HeavisideProjection::validParams()
{
  InputParameters params = FilterBase::validParams();
  params.addClassDescription(
      "Density filter followed by Heaviside projection for SIMP topology optimization");
  params.addRequiredCoupledVar("design_density", "Design density variable name.");
  params.addRequiredCoupledVar("filtered_density", "Filtered density variable name.");
  params.addRequiredCoupledVar("physical_density", "Physical density variable name.");
  params.set<MooseEnum>("filter_type") = "heaviside";
  params.set<ExecFlagEnum>("execute_on") = {EXEC_INITIAL, EXEC_TIMESTEP_END};
  params.set<bool>("force_postaux") = true;
  params.set<int>("execution_order_group") = 3;
  return params;
}

HeavisideProjection::HeavisideProjection(const InputParameters & parameters)
  : FilterBase(parameters),
    _design_density(&writableVariable("design_density")),
    _filtered_density(&writableVariable("filtered_density")),
    _physical_density(&writableVariable("physical_density"))
{
}

void
HeavisideProjection::initialize()
{
  gatherElementData();
  densityFilter();
  heavisideProjection();
}

void
HeavisideProjection::execute()
{
  // Grab the element data for each id
  auto elem_data_iter = _elem_data_map.find(_current_elem->id());

  // Check if the element data is not null
  mooseAssert(elem_data_iter != _elem_data_map.end(),
              "Element data not found for the current element id.");

  ElementData & elem_data = elem_data_iter->second;
  dynamic_cast<MooseVariableFE<Real> *>(_filtered_density)
      ->setNodalValue(elem_data.filtered_density);
  dynamic_cast<MooseVariableFE<Real> *>(_physical_density)
      ->setNodalValue(elem_data.projected_density);
}

void
HeavisideProjection::threadJoin(const UserObject & y)
{
  const HeavisideProjection & uo = static_cast<const HeavisideProjection &>(y);
  _elem_data_map.insert(uo._elem_data_map.begin(), uo._elem_data_map.end());
}

void
HeavisideProjection::gatherElementData()
{
  _elem_data_map.clear();

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();

      ElementData data = ElementData(
          dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem), 0, 0);
      _elem_data_map[elem_id] = data;
    }
}

void
HeavisideProjection::densityFilter()
{
  RealEigenVector density(_n_el);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    density(id) = elem_data.design_density;
  }

  RealEigenVector filtered = (_H * density).array() / _Hs.array();

  for (auto && [id, elem_data] : _elem_data_map)
  {
    elem_data.filtered_density = filtered(id);
  }
}

void
HeavisideProjection::heavisideProjection()
{
  for (auto && [id, elem_data] : _elem_data_map)
  {
    elem_data.projected_density =
        (std::tanh(_beta * _eta) + std::tanh(_beta * (elem_data.filtered_density - _eta))) /
        (std::tanh(_beta * _eta) + std::tanh(_beta * (1 - _eta)));
  }
}
