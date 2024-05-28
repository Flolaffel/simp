//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "DensityUpdateMMA.h"
#include "vector.h"
#include <algorithm>
#include "MooseVariableScalar.h"

registerMooseObject("OptimizationApp", DensityUpdateMMA);

InputParameters
DensityUpdateMMA::validParams()
{
  InputParameters params = ElementUserObject::validParams();
  params.addClassDescription("Computes updated densities based on objective function and "
                             "constraint sensitivities using MMA.");
  params.addRequiredCoupledVar("design_density", "Design density variable name.");
  params.addCoupledVar("old_design_density1", "Design density one iteration ago variable name.");
  params.addCoupledVar("old_design_density2", "Design density two iterations ago variable name.");
  params.addRequiredParam<VariableName>("objective_function_sensitivity",
                                        "Name of the objective function sensitivity variable.");
  params.addParam<std::vector<VariableName>>("constraint_values",
                                             "Constraint value variable names");
  params.addParam<std::vector<VariableName>>("constraint_sensitivities",
                                             "Constraint sensitivity variable names.");
  params.addCoupledVar("mma_lower_asymptotes",
                       "Column vector with the lower asymptotes from the previous "
                       "iteration (provided that iter>1).");
  params.addCoupledVar("mma_upper_asymptotes",
                       "Column vector with the upper asymptotes from the previous "
                       "iteration (provided that iter>1).");
  params.addParam<Real>("move_limit", 0.5, "Move limit.");
  params.set<int>("execution_order_group") = 2;
  return params;
}

DensityUpdateMMA::DensityUpdateMMA(const InputParameters & parameters)
  : ElementUserObject(parameters),
    _n_el(_mesh.getMesh().n_elem()),
    _design_density(&writableVariable("design_density")),
    _old_design_density1(&writableVariable("old_design_density1")),
    _old_design_density2(&writableVariable("old_design_density2")),
    _objective_sensitivity_name(getParam<VariableName>("objective_function_sensitivity")),
    _objective_sensitivity(&_subproblem.getStandardVariable(_tid, _objective_sensitivity_name)),
    _constraint_value_names(getParam<std::vector<VariableName>>("constraint_values")),
    _constraint_sensitivity_names(getParam<std::vector<VariableName>>("constraint_sensitivities")),
    _lower_asymptotes(&writableVariable("mma_lower_asymptotes")),
    _upper_asymptotes(&writableVariable("mma_upper_asymptotes")),
    _move_limit(getParam<Real>("move_limit"))
{
  if (!dynamic_cast<MooseVariableFE<Real> *>(_design_density))
    paramError("design_density", "Design density must be a finite element variable");

  if (isParamValid("constraint_values") && isParamValid("constraint_sensitivities"))
  {
    if (_constraint_value_names.size() != _constraint_sensitivity_names.size())
      mooseError("Please supply a value and a sensitivity for every constraint");
    _n_cons = _constraint_value_names.size();
    for (unsigned int i = 0; i < _n_cons; i++)
    {
      _constraint_values.push_back(
          &_subproblem.getScalarVariable(_tid, _constraint_value_names[i]));
      _constraint_sensitivities.push_back(
          &_subproblem.getStandardVariable(_tid, _constraint_sensitivity_names[i]));
    }
  }
}

void
DensityUpdateMMA::initialize()
{
  TIME_SECTION("initialize", 2, "Preparing Density Update");
  gatherElementData();
  performMmaLoop();
}

void
DensityUpdateMMA::execute()
{
  // Grab the element data for each id
  auto elem_data_iter = _elem_data_map.find(_current_elem->id());

  // Check if the element data is not null
  mooseAssert(elem_data_iter != _elem_data_map.end(),
              "Element data not found for the current element id.");

  ElementData & elem_data = elem_data_iter->second;
  dynamic_cast<MooseVariableFE<Real> *>(_design_density)
      ->setNodalValue(elem_data.new_design_density);
  dynamic_cast<MooseVariableFE<Real> *>(_old_design_density1)
      ->setNodalValue(elem_data.current_design_density);
  dynamic_cast<MooseVariableFE<Real> *>(_old_design_density2)
      ->setNodalValue(elem_data.old_design_density1);
  dynamic_cast<MooseVariableFE<Real> *>(_lower_asymptotes)->setNodalValue(elem_data.new_lower);
  dynamic_cast<MooseVariableFE<Real> *>(_upper_asymptotes)->setNodalValue(elem_data.new_upper);
}

void
DensityUpdateMMA::gatherElementData()
{
  TIME_SECTION("gatherElementData", 3, "Gathering Element Data");
  _elem_data_map.clear();

  for (const auto & sub_id : blockIDs())
    for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
    {
      dof_id_type elem_id = elem->id();

      std::vector<Real> con_sens(_n_cons);
      int i = 0;
      for (auto & sensitivity : _constraint_sensitivities)
      {
        con_sens[i] = dynamic_cast<MooseVariableFE<Real> *>(sensitivity)->getElementalValue(elem);
        i++;
      }

      ElementData data;
      data = ElementData(
          dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem),
          dynamic_cast<MooseVariableFE<Real> *>(_old_design_density1)->getElementalValue(elem),
          dynamic_cast<MooseVariableFE<Real> *>(_old_design_density2)->getElementalValue(elem),
          dynamic_cast<const MooseVariableFE<Real> *>(_objective_sensitivity)
              ->getElementalValue(elem),
          con_sens,
          dynamic_cast<MooseVariableFE<Real> *>(_lower_asymptotes)->getElementalValue(elem),
          dynamic_cast<MooseVariableFE<Real> *>(_upper_asymptotes)->getElementalValue(elem),
          elem->volume(),
          0,
          0,
          0);
      _elem_data_map[elem_id] = data;
    }
}

void
DensityUpdateMMA::performMmaLoop()
{
  TIME_SECTION("performMmaLoop", 3, "Building MMA Subproblem");
  unsigned int m = _n_cons;
  unsigned int n = _n_el;

  // Vector variables of size n
  std::vector<Real> xmin(n), xmax(n, 1);
  std::vector<Real> xval, xold1, xold2, df0dx, low, upp;
  std::vector<std::pair<dof_id_type, Real>> comm_xval, comm_df0dx, comm_xold1, comm_xold2, comm_low,
      comm_upp;

  // Vector variables of size m
  std::vector<Real> fval(m), a(m), c_MMA(m, 10000), d(m, 1);

  // Matrix variable of size m x n
  std::vector<std::vector<Real>> dfdx(m);
  std::vector<std::vector<std::pair<dof_id_type, Real>>> comm_dfdx(m);

  // Scalar constants
  Real a0 = 1;

  // Loop over all elements to populate the vectors
  for (auto && [id, elem_data] : _elem_data_map)
  {
    comm_xval.emplace_back(id, elem_data.current_design_density);
    comm_xold1.emplace_back(id, elem_data.old_design_density1);
    comm_xold2.emplace_back(id, elem_data.old_design_density2);
    /*f0val not needed*/
    comm_df0dx.emplace_back(id, elem_data.objective_sensitivity);
    for (unsigned int i = 0; i < m; i++)
    {
      comm_dfdx[i].emplace_back(id, elem_data.constraint_sensitivities[i]);
    }
    comm_low.emplace_back(id, elem_data.lower);
    comm_upp.emplace_back(id, elem_data.upper);
  }

  for (unsigned int i = 0; i < m; i++)
  {
    fval[i] = *(_constraint_values[0]->sln().data());
  }

  /// MPI communication
  if (_app.n_processors() > 1)
  {
    _communicator.allgather(comm_xval, false);
    _communicator.allgather(comm_xold1, false);
    _communicator.allgather(comm_xold2, false);
    _communicator.allgather(comm_df0dx, false);
    for (unsigned int i = 0; i < m; i++)
    {
      _communicator.allgather(comm_dfdx[i], false);
    }
    _communicator.allgather(comm_low, false);
    _communicator.allgather(comm_upp, false);

    std::sort(comm_xval.begin(), comm_xval.end());
    std::sort(comm_xold1.begin(), comm_xold1.end());
    std::sort(comm_xold2.begin(), comm_xold2.end());
    std::sort(comm_df0dx.begin(), comm_df0dx.end());
    for (unsigned int i = 0; i < m; i++)
    {
      std::sort(comm_dfdx[i].begin(), comm_dfdx[i].end());
    }
    std::sort(comm_low.begin(), comm_low.end());
    std::sort(comm_upp.begin(), comm_upp.end());
  }

  std::transform(comm_xval.begin(),
                 comm_xval.end(),
                 std::back_inserter(xval),
                 [](const std::pair<dof_id_type, Real> & p) { return p.second; });
  std::transform(comm_xold1.begin(),
                 comm_xold1.end(),
                 std::back_inserter(xold1),
                 [](const std::pair<dof_id_type, Real> & p) { return p.second; });
  std::transform(comm_xold2.begin(),
                 comm_xold2.end(),
                 std::back_inserter(xold2),
                 [](const std::pair<dof_id_type, Real> & p) { return p.second; });
  std::transform(comm_df0dx.begin(),
                 comm_df0dx.end(),
                 std::back_inserter(df0dx),
                 [](const std::pair<dof_id_type, Real> & p) { return p.second; });
  for (unsigned int i = 0; i < m; i++)
  {
    std::transform(comm_dfdx[i].begin(),
                   comm_dfdx[i].end(),
                   std::back_inserter(dfdx[i]),
                   [](const std::pair<dof_id_type, Real> & p) { return p.second; });
  }
  std::transform(comm_low.begin(),
                 comm_low.end(),
                 std::back_inserter(low),
                 [](const std::pair<dof_id_type, Real> & p) { return p.second; });
  std::transform(comm_upp.begin(),
                 comm_upp.end(),
                 std::back_inserter(upp),
                 [](const std::pair<dof_id_type, Real> & p) { return p.second; });

  /// MMA
  // NOTE: could be parametrized if needed
  Real epsimin = 0.0000001;
  Real raa0 = 0.00001;
  Real move = _move_limit;
  Real albefa = 0.1;
  Real asyinit = 0.5;
  Real asyincr = 1.2;
  Real asydecr = 0.7;
  std::vector<Real> eeen(n, 1);
  std::vector<Real> eeem(m, 1);

  // Calculation of the asymptotes low and upp
  std::vector<Real> zzz(n, 0);
  if (_t_step <= 2)
  {
    for (unsigned int i = 0; i < n; i++)
    {
      low[i] = xval[i] - asyinit * (xmax[i] - xmin[i]);
      upp[i] = xval[i] + asyinit * (xmax[i] - xmin[i]);
    }
  }
  else
  {
    for (unsigned int i = 0; i < n; i++)
    {
      zzz[i] = (xval[i] - xold1[i]) * (xold1[i] - xold2[i]);
      Real factor;
      if (zzz[i] > 0)
        factor = asyincr;
      else
        factor = asydecr;
      low[i] = xval[i] - factor * (xold1[i] - low[i]);
      upp[i] = xval[i] + factor * (upp[i] - xold1[i]);
      Real lowmin, lowmax, uppmin, uppmax;
      lowmin = xval[i] - 10 * (xmax[i] - xmin[i]);
      lowmax = xval[i] - 0.01 * (xmax[i] - xmin[i]);
      uppmin = xval[i] + 0.01 * (xmax[i] - xmin[i]);
      uppmax = xval[i] + 10 * (xmax[i] - xmin[i]);
      low[i] = std::max(low[i], lowmin);
      low[i] = std::min(low[i], lowmax);
      upp[i] = std::min(upp[i], uppmax);
      upp[i] = std::max(upp[i], uppmin);
    }
  }

  // Calculation of the bounds alpha and beta
  std::vector<Real> alpha(n, 0), beta(n, 0);
  for (unsigned int i = 0; i < n; i++)
  {
    Real zzz1, zzz2;
    zzz1 = low[i] + albefa * (xval[i] - low[i]);
    zzz2 = xval[i] - move * (xmax[i] - xmin[i]);
    zzz[i] = std::max(zzz1, zzz2);
    alpha[i] = std::max(zzz[i], xmin[i]);
    zzz1 = upp[i] - albefa * (upp[i] - xval[i]);
    zzz2 = xval[i] + move * (xmax[i] - xmin[i]);
    zzz[i] = std::min(zzz1, zzz2);
    beta[i] = std::min(zzz[i], xmax[i]);
  }

  // Calculations of p0, q0, P, Q and b
  std::vector<Real> xmami(n), xmamieps(n), xmamiinv(n), ux1(n), ux2(n), xl1(n), xl2(n), uxinv(n),
      xlinv(n), p0(n), q0(n), pq0(n);
  for (unsigned int i = 0; i < n; i++)
  {
    xmami[i] = xmax[i] - xmin[i];
    xmamieps[i] = 0.00001 * eeen[i];
    xmami[i] = std::max(xmami[i], xmamieps[i]);
    xmamiinv[i] = eeen[i] / xmami[i];
    ux1[i] = upp[i] - xval[i];
    ux2[i] = ux1[i] * ux1[i];
    xl1[i] = xval[i] - low[i];
    xl2[i] = xl1[i] * xl1[i];
    uxinv[i] = eeen[i] / ux1[i];
    xlinv[i] = eeen[i] / xl1[i];

    p0[i] = std::max(df0dx[i], 0.0);
    q0[i] = std::max(-df0dx[i], 0.0);
    pq0[i] = 0.001 * (p0[i] + q0[i]) + raa0 * xmamiinv[i];
    p0[i] += pq0[i];
    q0[i] += pq0[i];
    p0[i] *= ux2[i];
    q0[i] *= xl2[i];
  }

  std::vector<std::vector<Real>> P(m, std::vector<Real>(n)), Q(m, std::vector<Real>(n));
  std::vector<Real> b(m, 0);
  Real PQ;
  for (unsigned int i = 0; i < m; i++)
  {
    for (unsigned int j = 0; j < n; j++)
    {
      P[i][j] = std::max(dfdx[i][j], 0.0);
      Q[i][j] = std::max(-dfdx[i][j], 0.0);
      PQ = 0.001 * (P[i][j] + Q[i][j]) + raa0 * eeem[i] * xmamiinv[j];
      P[i][j] += PQ;
      Q[i][j] += PQ;
      P[i][j] *= ux2[j];
      Q[i][j] *= xl2[j];
      b[i] += P[i][j] * uxinv[j] + Q[i][j] * xlinv[j];
    }
    b[i] -= fval[i];
  }

  // Solving the subproblem by a primal-dual Newton method
  std::vector<Real> new_density =
      MmaSubSolve(m, n, epsimin, low, upp, alpha, beta, p0, q0, P, Q, a0, a, b, c_MMA, d);

  for (auto && [id, elem_data] : _elem_data_map)
  {
    // Update the element data values
    elem_data.new_design_density = new_density[id];
    elem_data.new_lower = low[id];
    elem_data.new_upper = upp[id];
  }
}

// Method to solve the MMA subproblem by a primal-dual Newton method
std::vector<Real>
DensityUpdateMMA::MmaSubSolve(unsigned int m,
                              unsigned int n,
                              Real epsimin,
                              std::vector<Real> low,
                              std::vector<Real> upp,
                              std::vector<Real> alpha,
                              std::vector<Real> beta,
                              std::vector<Real> p0,
                              std::vector<Real> q0,
                              std::vector<std::vector<Real>> P,
                              std::vector<std::vector<Real>> Q,
                              Real a0,
                              std::vector<Real> a,
                              std::vector<Real> b,
                              std::vector<Real> c,
                              std::vector<Real> d)
{
  TIME_SECTION("MmaSubSolve", 3, "Solving MMA Subproblem");
  std::vector<Real> een(n, 1);
  std::vector<Real> eem(m, 1);
  Real epsi = 1;
  std::vector<Real> epsvecn(n, 1);
  std::vector<Real> epsvecm(m, 1);
  std::vector<Real> y(m, 1);
  Real z = 1.0;
  std::vector<Real> lam(m, 1);
  Real zet = 1;
  std::vector<Real> s(m, 1);

  std::vector<Real> x(n, 0), xsi(n, 0), eta(n, 0);
  for (unsigned int i = 0; i < n; i++)
  {
    epsvecn[i] = epsi * een[i];
    x[i] = 0.5 * (alpha[i] + beta[i]);
    xsi[i] = een[i] / (x[i] - alpha[i]);
    xsi[i] = std::max(xsi[i], een[i]);
    eta[i] = een[i] / (beta[i] - x[i]);
    eta[i] = std::max(eta[i], een[i]);
  }

  std::vector<Real> mu(m);
  for (unsigned int i = 0; i < m; i++)
    mu[i] = std::max(eem[i], 0.5 * c[i]);

  while (epsi > epsimin)
  {
    std::vector<Real> ux1(n, 0), xl1(n, 0), ux2(n, 0), xl2(n, 0), uxinv1(n, 0), xlinv1(n, 0),
        rexsi(n, 0), reeta(n, 0);
    for (unsigned int i = 0; i < n; i++)
    {
      epsvecn[i] = epsi * een[i];
      ux1[i] = upp[i] - x[i];
      xl1[i] = x[i] - low[i];
      ux2[i] = ux1[i] * ux1[i];
      xl2[i] = xl1[i] * xl1[i];
      uxinv1[i] = een[i] / ux1[i];
      xlinv1[i] = een[i] / xl1[i];
      rexsi[i] = xsi[i] * (x[i] - alpha[i]) - epsvecn[i];
      reeta[i] = eta[i] * (beta[i] - x[i]) - epsvecn[i];
    }

    std::vector<Real> plam(n), qlam(n);
    std::vector<Real> gvec(m, 0);
    for (unsigned int i = 0; i < m; i++)
    {
      epsvecm[i] = epsi * eem[i];
      for (unsigned int j = 0; j < n; j++)
      {
        if (i == 0)
        {
          plam[j] = p0[j];
          qlam[j] = q0[j];
        }
        plam[j] += P[i][j] * lam[i];
        qlam[j] += Q[i][j] * lam[i];
        gvec[i] += P[i][j] * uxinv1[j] + Q[i][j] * xlinv1[j];
      }
    }

    std::vector<Real> dpsidx(n, 0), rex(n, 0);
    for (unsigned int i = 0; i < n; i++)
    {
      dpsidx[i] = plam[i] / ux2[i] - qlam[i] / xl2[i];
      rex[i] = dpsidx[i] - xsi[i] + eta[i];
    }

    std::vector<Real> rey(m), relam(m), remu(m), res(m);
    Real rez = 0;
    for (unsigned int i = 0; i < m; i++)
    {
      rey[i] = c[i] + d[i] * y[i] - mu[i] - lam[i];
      relam[i] = gvec[i] - a[i] * z - y[i] + s[i] - b[i];
      remu[i] = mu[i] * y[i] - epsvecm[i];
      res[i] = lam[i] * s[i] - epsvecm[i];
      rez -= a[i] * lam[i];
    }
    rez = rez + a0 - zet;
    Real rezet = zet * z - epsi;

    std::vector<Real> residu1;
    residu1.reserve(rex.size() + rey.size() + 1);
    residu1.assign(std::begin(rex), std::end(rex));
    residu1.insert(std::end(residu1), std::begin(rey), std::end(rey));
    residu1.push_back(rez);

    std::vector<Real> residu2;
    residu2.reserve(relam.size() + residu2.size() + rexsi.size() + reeta.size() + 3);
    residu2.assign(std::begin(relam), std::end(relam));
    residu2.insert(std::end(residu2), std::begin(rexsi), std::end(rexsi));
    residu2.insert(std::end(residu2), std::begin(reeta), std::end(reeta));
    residu2.insert(std::end(residu2), std::begin(remu), std::end(remu));
    residu2.push_back(rezet);
    residu2.insert(std::end(residu2), std::begin(res), std::end(res));

    std::vector<Real> residu;
    residu.reserve(residu1.size() + residu2.size());
    residu.assign(std::begin(residu1), std::end(residu1));
    residu.insert(std::end(residu), std::begin(residu2), std::end(residu2));

    Real residunorm = NormVec(residu);
    std::vector<Real> residuabs = AbsVec(residu);
    Real residumax = *std::max_element(std::begin(residuabs), std::end(residuabs));
    int ittt = 0;
    while (residumax > 0.9 * epsi && ittt < 200)
    {
      ittt++;
      std::vector<std::vector<Real>> GG(m, std::vector<Real>(n));
      std::vector<Real> ux3(n, 0), xl3(n, 0), uxinv2(n, 0), xlinv2(n, 0), delx(n, 0), diagx(n, 0),
          diagxinv(n, 0);
      for (unsigned int i = 0; i < n; i++)
      {
        ux1[i] = upp[i] - x[i];
        xl1[i] = x[i] - low[i];
        ux2[i] = ux1[i] * ux1[i];
        xl2[i] = xl1[i] * xl1[i];
        ux3[i] = ux1[i] * ux2[i];
        xl3[i] = xl1[i] * xl2[i];
        uxinv1[i] = een[i] / ux1[i];
        xlinv1[i] = een[i] / xl1[i];
        uxinv2[i] = een[i] / ux2[i];
        xlinv2[i] = een[i] / xl2[i];
      }

      std::fill(std::begin(plam), std::end(plam), 0);
      std::fill(std::begin(qlam), std::end(qlam), 0);
      std::fill(std::begin(gvec), std::end(gvec), 0);
      for (unsigned int i = 0; i < m; i++)
      {
        for (unsigned int j = 0; j < n; j++)
        {
          if (i == 0)
          {
            plam[j] = p0[j];
            qlam[j] = q0[j];
          }
          plam[j] += P[i][j] * lam[i];
          qlam[j] += Q[i][j] * lam[i];
          gvec[i] += P[i][j] * uxinv1[j] + Q[i][j] * xlinv1[j];
          GG[i][j] = P[i][j] * uxinv2[j] - Q[i][j] * xlinv2[j];
        }
      }

      for (unsigned int i = 0; i < n; i++)
      {
        dpsidx[i] = plam[i] / ux2[i] - qlam[i] / xl2[i];
        delx[i] = dpsidx[i] - epsvecn[i] / (x[i] - alpha[i]) + epsvecn[i] / (beta[i] - x[i]);
        diagx[i] = plam[i] / ux3[i] + qlam[i] / xl3[i];
        diagx[i] = 2 * diagx[i] + xsi[i] / (x[i] - alpha[i]) + eta[i] / (beta[i] - x[i]);
        diagxinv[i] = een[i] / diagx[i];
      }

      std::vector<Real> dely(m), dellam(m), diagy(m), diagyinv(m), diaglam(m), diaglamyi(m);
      Real delz = 0;
      for (unsigned int i = 0; i < m; i++)
      {
        dely[i] = c[i] + d[i] * y[i] - lam[i] - epsvecm[i] / y[i];
        dellam[i] = gvec[i] - a[i] * z - y[i] - b[i] + epsvecm[i] / lam[i];
        diagy[i] = d[i] + mu[i] / y[i];
        diagyinv[i] = eem[i] / diagy[i];
        diaglam[i] = s[i] / lam[i];
        diaglamyi[i] = diaglam[i] + diagyinv[i];
        delz -= a[i] * lam[i];
      }
      delz = delz + a0 - epsi / z;

      std::vector<std::vector<Real>> Alam(m, std::vector<Real>(m));
      std::vector<Real> dx(n, 0);
      std::vector<Real> dlam;
      Real dz;
      if (m < n)
      {
        std::vector<Real> blam(m);
        for (unsigned int i = 0; i < m; i++)
        {
          blam[i] = dellam[i] + dely[i] / diagy[i];
          for (unsigned int j = 0; j < m; j++)
          {
            if (i == j)
              Alam[i][j] = diaglamyi[i];
            for (unsigned int k = 0; k < n; k++)
            {
              blam[i] -= GG[i][k] * (delx[k] / diagx[k]);
              Alam[i][j] += GG[i][k] * diagxinv[k] * GG[j][k];
            }
          }
        }

        std::vector<Real> bb = blam;
        bb.push_back(delz);
        std::vector<std::vector<Real>> AA(m + 1, std::vector<Real>(m + 1));
        for (unsigned int i = 0; i < m + 1; i++)
        {
          for (unsigned int j = 0; j < m + 1; j++)
          {
            if (i < m && j < m)
              AA[i][j] = Alam[i][j];
            else if (i == m && j == m)
              AA[i][j] = -zet / z;
            else if (i < j)
              AA[i][j] = a[i];
            else if (i > j)
              AA[i][j] = a[j];
          }
        }
        std::vector<std::vector<Real>> AAinv = getInverse(AA);
        std::vector<Real> solut(m + 1);
        for (unsigned int i = 0; i < AAinv.size(); i++)
        {
          for (unsigned int j = 0; j < AAinv[0].size(); j++)
          {
            solut[i] += AAinv[i][j] * bb[j];
          }
        }
        dlam.assign(std::begin(solut), std::next(std::begin(solut), m));
        dz = solut[m];
        for (unsigned int i = 0; i < n; i++)
        {
          Real temp = 0;
          for (unsigned int j = 0; j < m; j++)
          {
            temp += GG[j][i] * dlam[j];
          }
          dx[i] = -delx[i] / diagx[i] - temp / diagx[i];
        }
      }
      else
      {
        // idk
      }

      std::vector<Real> dxsi(n, 0), deta(n, 0);
      for (unsigned int i = 0; i < n; i++)
      {
        dxsi[i] = -xsi[i] + epsvecn[i] / (x[i] - alpha[i]) - (xsi[i] * dx[i]) / (x[i] - alpha[i]);
        deta[i] = -eta[i] + epsvecn[i] / (beta[i] - x[i]) + (eta[i] * dx[i]) / (beta[i] - x[i]);
      }

      std::vector<Real> dy(m), dmu(m), ds(m);
      for (unsigned int i = 0; i < m; i++)
      {
        dy[i] = -dely[i] / diagy[i] + dlam[i] / diagy[i];
        dmu[i] = -mu[i] + epsvecm[i] / y[i] - (mu[i] * dy[i]) / y[i];
        ds[i] = -s[i] + epsvecm[i] / lam[i] - (s[i] * dlam[i]) / lam[i];
      }

      Real dzet = -zet + epsi / z - zet * dz / z;

      std::vector<Real> xx;
      xx.reserve(y.size() + 1 + lam.size() + xsi.size() + eta.size() + mu.size() + 1 + s.size());
      xx.assign(std::begin(y), std::end(y));
      xx.push_back(z);
      xx.insert(std::end(xx), std::begin(lam), std::end(lam));
      xx.insert(std::end(xx), std::begin(xsi), std::end(xsi));
      xx.insert(std::end(xx), std::begin(eta), std::end(eta));
      xx.insert(std::end(xx), std::begin(mu), std::end(mu));
      xx.push_back(zet);
      xx.insert(std::end(xx), std::begin(s), std::end(s));

      std::vector<Real> dxx;
      dxx.reserve(dy.size() + 1 + dlam.size() + dxsi.size() + deta.size() + dmu.size() + 1 +
                  ds.size());
      dxx.assign(std::begin(dy), std::end(dy));
      dxx.push_back(dz);
      dxx.insert(std::end(dxx), std::begin(dlam), std::end(dlam));
      dxx.insert(std::end(dxx), std::begin(dxsi), std::end(dxsi));
      dxx.insert(std::end(dxx), std::begin(deta), std::end(deta));
      dxx.insert(std::end(dxx), std::begin(dmu), std::end(dmu));
      dxx.push_back(dzet);
      dxx.insert(std::end(dxx), std::begin(ds), std::end(ds));

      std::vector<Real> stepxx(xx.size());
      for (unsigned int i = 0; i < xx.size(); i++)
      {
        stepxx[i] = -1.01 * dxx[i] / xx[i];
      }
      std::vector<Real> stepalpha(n), stepbeta(n);
      for (unsigned int i = 0; i < n; i++)
      {
        stepalpha[i] = -1.01 * dx[i] / (x[i] - alpha[i]);
        stepbeta[i] = 1.01 * dx[i] / (beta[i] - x[i]);
      }
      Real stmxx = *std::max_element(std::begin(stepxx), std::end(stepxx));
      Real stmalpha = *std::max_element(std::begin(stepalpha), std::end(stepalpha));
      Real stmbeta = *std::max_element(std::begin(stepbeta), std::end(stepbeta));
      Real stmalbe = std::max(stmalpha, stmbeta);
      Real stmalbexx = std::max(stmalbe, stmxx);
      Real stminv = std::max(stmalbexx, 1.0);
      Real steg = 1.0 / stminv;

      std::vector<Real> xold = x;
      std::vector<Real> yold = y;
      Real zold = z;
      std::vector<Real> lamold = lam;
      std::vector<Real> xsiold = xsi;
      std::vector<Real> etaold = eta;
      std::vector<Real> muold = mu;
      Real zetold = zet;
      std::vector<Real> sold = s;

      int itto = 0;
      Real resinew = 2 * residunorm;
      while (resinew > residunorm && itto < 50)
      {
        itto++;
        for (unsigned int i = 0; i < n; i++)
        {
          x[i] = xold[i] + steg * dx[i];
          xsi[i] = xsiold[i] + steg * dxsi[i];
          eta[i] = etaold[i] + steg * deta[i];
        }
        for (unsigned int i = 0; i < m; i++)
        {
          y[i] = yold[i] + steg * dy[i];
          lam[i] = lamold[i] + steg * dlam[i];
          mu[i] = muold[i] + steg * dmu[i];
          s[i] = sold[i] + steg * ds[i];
        }
        z = zold + steg * dz;
        zet = zetold + steg * dzet;

        for (unsigned int i = 0; i < n; i++)
        {
          epsvecn[i] = epsi * een[i];
          ux1[i] = upp[i] - x[i];
          xl1[i] = x[i] - low[i];
          ux2[i] = ux1[i] * ux1[i];
          xl2[i] = xl1[i] * xl1[i];
          uxinv1[i] = een[i] / ux1[i];
          xlinv1[i] = een[i] / xl1[i];
          rexsi[i] = xsi[i] * (x[i] - alpha[i]) - epsvecn[i];
          reeta[i] = eta[i] * (beta[i] - x[i]) - epsvecn[i];
        }

        std::fill(std::begin(plam), std::end(plam), 0);
        std::fill(std::begin(qlam), std::end(qlam), 0);
        std::fill(std::begin(gvec), std::end(gvec), 0);
        for (unsigned int i = 0; i < m; i++)
        {
          for (unsigned int j = 0; j < n; j++)
          {
            if (i == 0)
            {
              plam[j] = p0[j];
              qlam[j] = q0[j];
            }
            plam[j] += P[i][j] * lam[i];
            qlam[j] += Q[i][j] * lam[i];
            gvec[i] += P[i][j] * uxinv1[j] + Q[i][j] * xlinv1[j];
          }
        }

        for (unsigned int i = 0; i < n; i++)
        {
          dpsidx[i] = plam[i] / ux2[i] - qlam[i] / xl2[i];
          rex[i] = dpsidx[i] - xsi[i] + eta[i];
        }

        rez = 0;
        for (unsigned int i = 0; i < m; i++)
        {
          rey[i] = c[i] + d[i] * y[i] - mu[i] - lam[i];
          relam[i] = gvec[i] - a[i] * z - y[i] + s[i] - b[i];
          remu[i] = mu[i] * y[i] - epsvecm[i];
          res[i] = lam[i] * s[i] - epsvecm[i];
          rez -= a[i] * lam[i];
        }
        rez = rez + a0 - zet;
        rezet = zet * z - epsi;

        residu1.assign(std::begin(rex), std::end(rex));
        residu1.insert(std::end(residu1), std::begin(rey), std::end(rey));
        residu1.push_back(rez);

        residu2.assign(std::begin(relam), std::end(relam));
        residu2.insert(std::end(residu2), std::begin(rexsi), std::end(rexsi));
        residu2.insert(std::end(residu2), std::begin(reeta), std::end(reeta));
        residu2.insert(std::end(residu2), std::begin(remu), std::end(remu));
        residu2.push_back(rezet);
        residu2.insert(std::end(residu2), std::begin(res), std::end(res));

        residu.assign(std::begin(residu1), std::end(residu1));
        residu.insert(std::end(residu), std::begin(residu2), std::end(residu2));

        resinew = NormVec(residu);
        steg /= 2;
      }
      residunorm = resinew;
      std::vector<Real> residuabs = AbsVec(residu);
      residumax = *std::max_element(std::begin(residuabs), std::end(residuabs));
    }
    epsi *= 0.1;
  }
  return x;
}
