//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "SensitivityUpdateDensityFilter.h"
#include "MooseError.h"
#include <algorithm>

registerMooseObject("OptimizationApp", SensitivityUpdateDensityFilter);

InputParameters
SensitivityUpdateDensityFilter::validParams()
{
  InputParameters params = ElementUserObject::validParams();
  params.addClassDescription(
      "Computes the filtered sensitivity using a radial average user object.");
  params.addRequiredCoupledVar("compliance_sensitivity",
                               "Name of the compliance_sensitivity variable.");
  params.addRequiredCoupledVar("volume_sensitivity", "Name of the volume_sensitivity variable.");
  params.addRequiredParam<Real>("radius", "Cut-off radius for the averaging");
  params.addRequiredParam<MeshGeneratorName>(
      "mesh_generator",
      "Name of the mesh generator to be used to retrieve control drums information.");

  return params;
}

SensitivityUpdateDensityFilter::SensitivityUpdateDensityFilter(const InputParameters & parameters)
  : ElementUserObject(parameters),
    _mesh(_subproblem.mesh()),
    _mesh_generator(getParam<MeshGeneratorName>("mesh_generator")),
    _compliance_sensitivity(&writableVariable("compliance_sensitivity")),
    //_compliance_sensitivity_name(getParam<VariableName>("compliance_sensitivity")),
    _volume_sensitivity(&writableVariable("volume_sensitivity")),
    //_volume_sensitivity_name(getParam<VariableName>("volume_sensitivity")),
    _radius(getParam<Real>("radius")),
    _nx(getMeshProperty<unsigned int>("num_elements_x", _mesh_generator)),
    _ny(getMeshProperty<unsigned int>("num_elements_y", _mesh_generator)),
    _xmin(getMeshProperty<Real>("xmin", _mesh_generator)),
    _xmax(getMeshProperty<Real>("xmax", _mesh_generator)),
    _ymin(getMeshProperty<Real>("ymin", _mesh_generator)),
    _ymax(getMeshProperty<Real>("ymax", _mesh_generator))
{
}

void
SensitivityUpdateDensityFilter::initialize()
{
  gatherElementData();
  prepareFilter();
  filterSensitivities();
}

void
SensitivityUpdateDensityFilter::execute()
{
  // Grab the element data for each id
  auto elem_data_iter = _elem_data_map.find(_current_elem->id());

  // Check if the element data is not null
  if (elem_data_iter != _elem_data_map.end())
  {
    ElementData & elem_data = elem_data_iter->second;
    dynamic_cast<MooseVariableFE<Real> *>(_compliance_sensitivity)
        ->setNodalValue(elem_data.new_compliance_sensitivity);
    dynamic_cast<MooseVariableFE<Real> *>(_volume_sensitivity)
        ->setNodalValue(elem_data.new_volume_sensitivity);
  }
  else
  {
    mooseError("Element data not found for the current element id.");
  }
}

void
SensitivityUpdateDensityFilter::gatherElementData()
{
  _elem_data_map.clear();

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();
      ElementData data = ElementData(
          dynamic_cast<MooseVariableFE<Real> *>(_compliance_sensitivity)->getElementalValue(elem),
          dynamic_cast<MooseVariableFE<Real> *>(_volume_sensitivity)->getElementalValue(elem),
          0,
          0);
      _elem_data_map[elem_id] = data;
    }
}

void
SensitivityUpdateDensityFilter::prepareFilter()
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
SensitivityUpdateDensityFilter::filterSensitivities()
{
  std::vector<Real> mod_dc(_nx * _ny);
  std::vector<Real> mod_dv(_nx * _ny);
  for (auto && [id, elem_data] : _elem_data_map)
  {
    mod_dc[id] = elem_data.compliance_sensitivity / _Hs[id];
    mod_dv[id] = elem_data.volume_sensitivity / _Hs[id];
  }

  for (auto && [id, elem_data] : _elem_data_map)
  {
    Real filt_dc = 0;
    Real filt_dv = 0;
    for (unsigned int j = 0; j < _nx * _ny; j++)
    {
      filt_dc += _H[id][j] * mod_dc[j];
      filt_dv += _H[id][j] * mod_dv[j];
    }
    elem_data.new_compliance_sensitivity = filt_dc;
    elem_data.new_volume_sensitivity = filt_dv;
  }
}
