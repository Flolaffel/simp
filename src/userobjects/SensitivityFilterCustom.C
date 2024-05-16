//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "SensitivityFilterCustom.h"
#include "MooseError.h"
#include <algorithm>

registerMooseObject("OptimizationApp", SensitivityFilterCustom);

InputParameters
SensitivityFilterCustom::validParams()
{
  InputParameters params = FilterBase::validParams();
  params.addClassDescription("Computes the filtered sensitivities for "
                             "sensitivity filtering, density filtering or Heaviside projection.");
  params.addRequiredParam<UserObjectName>("map_UO", "Radial Average user object");
  params.addRequiredCoupledVar("sensitivities", "Name of the sensitivity variables.");
  params.addParam<VariableName>("design_density", "Design density variable name.");
  params.addParam<VariableName>("filtered_density", "Filtered density variable name.");
  params.set<bool>("force_postaux") = true;
  params.set<int>("execution_order_group") = 1;
  return params;
}

SensitivityFilterCustom::SensitivityFilterCustom(const InputParameters & parameters)
  : FilterBase(parameters),
    _n_vars(coupledComponents("sensitivities")),
    _map(getUserObject<GatherElementData>("map_UO").getMap())
{
  for (unsigned int i = 0; i < _n_vars; i++)
    _sensitivities.push_back(&writableVariable("sensitivities", i));

  if (_filter_type == FilterType::SENSITIVITY)
  {
    if (_n_vars > 1)
      mooseError("Classic sensitivity filter only requires the objective function sensitivity to "
                 "be supplied");
    _design_density_name = getParam<VariableName>("design_density");
    _design_density = &_subproblem.getStandardVariable(_tid, _design_density_name);
  }
  else if (_filter_type == FilterType::HEAVISIDE)
  {
    _filtered_density_name = getParam<VariableName>("filtered_density");
    _filtered_density = &_subproblem.getStandardVariable(_tid, _filtered_density_name);
  }
}

void
SensitivityFilterCustom::initialize()
{
  TIME_SECTION("initialize", 2, "Initialize SensitvityFilterCustom");
  if (_filter_type != FilterType::NONE)
  {
    gatherElementData();
    threadJoin(*this);
    if (_filter_type == FilterType::SENSITIVITY)
      updateSensitivitiesSensitivityFilter();
    else if (_filter_type == FilterType::DENSITY)
      updateSensitivitiesDensityFilter();
    else if (_filter_type == FilterType::HEAVISIDE)
      updateSensitivitiesHeaviside();
  }
}

void
SensitivityFilterCustom::execute()
{
  TIME_SECTION("execute", 3, "Execute SensitvityFilterCustom");
  if (_filter_type != FilterType::NONE)
  {
    // Grab the element data for each id
    auto elem_data_iter = _elem_data_map.find(_current_elem->id());

    // Check if the element data is not null
    mooseAssert(elem_data_iter != _elem_data_map.end(),
                "Element data not found for the current element id.");

    ElementData & elem_data = elem_data_iter->second;
    int i = 0;
    for (auto & sensitivity : _sensitivities)
    {
      dynamic_cast<MooseVariableFE<Real> *>(sensitivity)
          ->setNodalValue(elem_data.filtered_sensitivities[i]);
      i++;
    }
  }
}

void
SensitivityFilterCustom::threadJoin(const UserObject & y)
{
  TIME_SECTION("threadJoin", 3, "Joining Threads");
  const SensitivityFilterCustom & uo = static_cast<const SensitivityFilterCustom &>(y);
  _elem_data_map.insert(uo._elem_data_map.begin(), uo._elem_data_map.end());
}

void
SensitivityFilterCustom::gatherElementData()
{
  TIME_SECTION("gatherElementData", 3, "Gathering Element Data");
  _elem_data_map.clear();

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();

      std::vector<Real> sens_values(_n_vars);
      std::vector<Real> filt_values(_n_vars);
      int i = 0;
      for (auto & sensitivity : _sensitivities)
      {
        sens_values[i] =
            dynamic_cast<MooseVariableFE<Real> *>(sensitivity)->getElementalValue(elem);
        i++;
      }

      if (_filter_type == FilterType::SENSITIVITY)
      {
        ElementData data = ElementData(
            sens_values,
            dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem),
            0,
            filt_values);
        _elem_data_map[elem_id] = data;
      }
      else if (_filter_type == FilterType::DENSITY)
      {
        ElementData data = ElementData(sens_values, 1, 0, filt_values);
        _elem_data_map[elem_id] = data;
      }
      else if (_filter_type == FilterType::HEAVISIDE)
      {
        ElementData data = ElementData(
            sens_values,
            0,
            dynamic_cast<MooseVariableFE<Real> *>(_filtered_density)->getElementalValue(elem),
            filt_values);
        _elem_data_map[elem_id] = data;
      }
    }

  std::cout << _elem_data_map.size() << "\n\n";
  std::cout << _map.size() << "\n\n";
}

void
SensitivityFilterCustom::updateSensitivitiesSensitivityFilter()
{
  TIME_SECTION("updateSensitivitiesSensitivityFilter", 3, "Filtering Objective Sensitivity");
  RealEigenVector sens(_n_el), dens(_n_el);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    sens(id) = elem_data.sensitivities[0];
    dens(id) = elem_data.design_density;
  }

  RealEigenVector min = 0.001 * RealEigenVector::Ones(_n_el);
  RealEigenVector temp = min.cwiseMax(dens);
  RealEigenVector filt_sens =
      (_H * (dens.array() * sens.array()).matrix()).array() / _Hs.array() / temp.array();

  for (auto && [id, elem_data] : _elem_data_map)
  {
    elem_data.filtered_sensitivities[0] = filt_sens(id);
  }
}

void
SensitivityFilterCustom::updateSensitivitiesDensityFilter()
{
  TIME_SECTION("updateSensitivitiesDensityFilter", 3, "Updating Sensitivities");
  RealEigenMatrix temp_sens(_n_el, _n_vars);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    for (unsigned int var = 0; var < temp_sens.cols(); var++)
    {
      temp_sens(id, var) = elem_data.sensitivities[var] / _Hs(id);
    }
  }

  RealEigenMatrix filt_sens = _H * temp_sens;

  for (auto && [id, elem_data] : _elem_data_map)
  {
    std::vector<Real> assign(_n_vars);
    RealEigenVector::Map(&assign[0], _n_vars) = filt_sens.row(id);
    elem_data.filtered_sensitivities = assign;
  }
}

void
SensitivityFilterCustom::updateSensitivitiesHeaviside()
{
  TIME_SECTION("updateSensitivitiesHeaviside", 3, "Updating Sensitivities");
  RealEigenVector dx(_n_el);
  RealEigenMatrix sens(_n_el, _n_vars);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    dx(id) = (_beta * std::pow(1 / std::cosh(_beta * (elem_data.filtered_density - _eta)), 2)) /
             (std::tanh(_beta * _eta) + std::tanh(_beta * (1 - _eta)));
    for (unsigned int var = 0; var < _n_vars; var++)
    {
      sens(id, var) = elem_data.sensitivities[var];
    }
  }

  RealEigenMatrix filt_sens(_n_el, _n_vars);
  for (unsigned int var = 0; var < _n_vars; var++)
  {
    filt_sens.col(var) = _H * (sens.col(var).array() * dx.array() / _Hs.array()).matrix();
  }

  for (auto && [id, elem_data] : _elem_data_map)
  {
    std::vector<Real> assign(_n_vars);
    RealEigenVector::Map(&assign[0], _n_vars) = filt_sens.row(id);
    elem_data.filtered_sensitivities = assign;
  }
}
