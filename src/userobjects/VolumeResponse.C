//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "VolumeResponse.h"
#include "MooseVariableScalar.h"
#include <algorithm>

// #include "MooseVariableBase.h"

registerMooseObject("OptimizationApp", VolumeResponse);

InputParameters
VolumeResponse::validParams()
{
  InputParameters params = TODesignResponse::validParams();
  params.addClassDescription("Computes the volume sensitivities for volume constrained topology "
                             "optimization (2D ONLY). RELATIVE LIMIT");
  return params;
}

VolumeResponse::VolumeResponse(const InputParameters & parameters) : TODesignResponse(parameters) {}

void
VolumeResponse::initialize()
{
  TIME_SECTION("initialize", 2, "Initialize VolumeResponse");
  gatherElementData();
  computeValue();
  computeSensitivity();
}

void
VolumeResponse::execute()
{
  TIME_SECTION("execute", 3, "Writing Volume Sensitivity");
  // Grab the element data for each id
  auto elem_data_iter = _elem_data_map.find(_current_elem->id());

  // Check if the element data is not null
  mooseAssert(elem_data_iter != _elem_data_map.end(),
              "Element data not found for the current element id.");

  ElementData & elem_data = elem_data_iter->second;
  dynamic_cast<MooseVariableFE<Real> *>(_sensitivity)
      ->setNodalValue(elem_data.new_volume_sensitivity);
}

void
VolumeResponse::gatherElementData()
{
  TIME_SECTION("gatherElementData", 3, "Gather Element Data");
  _elem_data_map.clear();

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();

      ElementData data = ElementData(
          dynamic_cast<const MooseVariableFE<Real> *>(_physical_density)->getElementalValue(elem),
          0);
      _elem_data_map[elem_id] = data;
    }
}

void
VolumeResponse::computeValue()
{
  TIME_SECTION("computeValue", 3, "Computing Volume Value");
  Real value = 0;
  for (auto && [id, elem_data] : _elem_data_map)
  {
    value += elem_data.physical_density;
  }

  if (_is_objective)
    // min: V_rel = V / V_0
    value /= _n_el;
  else if (_is_constraint)
  {
    // g: V_rel / V_lim - 1 <= 0
    value /= _limit * _n_el;
    value -= 1;
  }

  _value->setValues(value);
  _value->insert(_value->sys().solution());
  _value->sys().solution().close();
}

void
VolumeResponse::computeSensitivity()
{
  TIME_SECTION("computeSensitivity", 3, "Computing Volume Sensitivity");
  for (auto && [id, elem_data] : _elem_data_map)
  {
    if (_is_objective)
      elem_data.new_volume_sensitivity = 1.0 / _n_el;
    else if (_is_constraint)
      elem_data.new_volume_sensitivity = 1.0 / (_limit * _n_el);
  }
}
