nx = 200
ny = 200
p = 3
vol_frac = 0.5
filter_radius = 1.5
E0 = 1
Emin = 1e-9


[GlobalParams]
  displacements = 'u_x u_y'
[]

[Mesh]
  [domain]
    type = GeneratedMeshGenerator
    dim = 2
    xmin = -50
    xmax = 50
    ymin = -50
    ymax = 50
    nx = ${nx}
    ny = ${ny}
  []
  [mid]
    type = ExtraNodesetGenerator
    input = domain
    new_boundary = mid
    coord = '0 0 0'
  []
[]

[AuxVariables]
  [Emin]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${Emin}
  []
  [p]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${p}
  []
  [E0]
    family = MONOMIAL
    order = CONSTANT
    initial_condition = ${E0}
  []
  [dc]
    family = MONOMIAL
    order = CONSTANT
  []
  [V]
    family = SCALAR
    order = FIRST
  []
  [dV]
    family = MONOMIAL
    order = CONSTANT
  []
  [rho]
    family = MONOMIAL
    order = CONSTANT
  []
  [rhoPhys]
    family = MONOMIAL
    order = CONSTANT
  []
  [rho_old1]
    family = MONOMIAL
    order = CONSTANT
  []
  [rho_old2]
    family = MONOMIAL
    order = CONSTANT
  []
  [low]
    family = MONOMIAL
    order = CONSTANT
  []
  [upp]
    family = MONOMIAL
    order = CONSTANT
  []
[]

[AuxKernels]
  [copy_compliance_sens]
    type = MaterialRealAux
    property = sensitivity
    variable = dc
    execute_on = TIMESTEP_END
  []
  [copy_rho]
    type = CopyValueAux
    source = rho
    variable = rhoPhys
    execute_on = TIMESTEP_BEGIN
  []
[]

[Variables]
  [global_strain]
    order = THIRD
    family = SCALAR
  []
[]

[Modules]
  [TensorMechanics]
    [Master]
      [stress_div]
        strain = SMALL
        add_variables = true
        incremental = false
        global_strain = global_strain
        generate_output = 'strain_xx strain_xy strain_yy stress_xx stress_xy
                           stress_yy vonmises_stress'
      []
    []
    [GlobalStrain]
      [global_strain]
        scalar_global_strain = global_strain
        applied_stress_tensor = '0 0 0 0 0 1e-4'
        displacements = 'u_x u_y'
        auxiliary_displacements = 'disp_x disp_y'
        global_displacements = 'ug_x ug_y'
      []
    []
  []
[]

[ICs]
  [rho_IC]
    type = BoundingBoxIC
    variable = rho
    x1 = -7.5
    x2 = 7.51
    y1 = -7.5
    y2 = 7.51
    inside = 0
    outside = ${vol_frac}
  []
  [rho_phys_IC]
    type = BoundingBoxIC
    variable = rhoPhys
    x1 = -7.5
    x2 = 7.51
    y1 = -7.5
    y2 = 7.51
    inside = 0
    outside = ${vol_frac}
  []
  [rho_old1_IC]
    type = BoundingBoxIC
    variable = rho_old1
    x1 = -7.5
    x2 = 7.51
    y1 = -7.5
    y2 = 7.51
    inside = 0
    outside = ${vol_frac}
  []
  [rho_old2_IC]
    type = BoundingBoxIC
    variable = rho_old2
    x1 = -7.5
    x2 = 7.51
    y1 = -7.5
    y2 = 7.51
    inside = 0
    outside = ${vol_frac}
  []
[]

[BCs]
  [Periodic]
    [all]
      auto_direction = 'x y'
      variable = ' u_x u_y'
    []
  []
  [fix_x]
    type = DirichletBC
    boundary = mid
    variable = u_x
    value = 0
  []
  [fix_y]
    type = DirichletBC
    boundary = mid
    variable = u_y
    value = 0
  []
[]

[Materials]
  [elasticity_tensor]
    type = ComputeVariableIsotropicElasticityTensor
    youngs_modulus = E_phys
    poissons_ratio = 0.3
    args = 'Emin rho p E0'
  []
  [E_phys]
    type = DerivativeParsedMaterial
    # Emin + (density^penal) * (E0 - Emin)
    expression = '${Emin} + (rho ^ ${p}) * (${E0}-${Emin})'
    coupled_variables = 'rho'
    property_name = E_phys
  []
  [dc]
    type = AnalyticComplianceSensitivity
    physical_density = rhoPhys
    youngs_modulus = E_phys
    incremental = false
    E0 = ${E0}
    Emin = ${Emin}
    p = ${p}
  []
  [compute_stress]
    type = ComputeLinearElasticStress
  []
[]

[UserObjects]
  [update]
    type = DensityUpdateMMA
    objective_function_sensitivity = dc
    constraint_values = 'V'
    constraint_sensitivities = dV
    design_density = rho
    old_design_density1 = rho_old1
    old_design_density2 = rho_old2
    mma_lower_asymptotes = low
    mma_upper_asymptotes = upp
  []
  # needs MaterialRealAux to copy sensitivity (mat prop) to dc aux variable
  [calc_sense]
    type = SensitivityFilterCustom
    sensitivities = 'dc'
    mesh_generator = domain
    design_density = rho
    filter_type = sensitivity
    radius = ${filter_radius}
  []
  [vol_sens]
    type = VolumeResponse
    usage = constraint
    limit = ${vol_frac}
    value = V
    sensitivity = dV
    physical_density = rhoPhys
  []
  [opt_conv]
    type = Terminator
    expression = 'change < 0.0075 & change != 0'
    execute_on = TIMESTEP_END
    execution_order_group = 5
  []
[]

[Postprocessors]
  [change]
    type = VectorPostprocessorComponent
    vectorpostprocessor = max_abs_diff
    vector_name = Difference
    index = 0
    execute_on = 'initial timestep_end'
  []
[]

[VectorPostprocessors]
  [max_abs_diff]
    type = ElementVariablesDifferenceMax
    compare_a = rho
    compare_b = rho_old1
    furthest_from_zero = true
    contains_complete_history = true
    execution_order_group = 10
  []
[]

[Preconditioning]
  [SMP]
    type = SMP
    full = true
  []
[]

[Executioner]
  type = Transient
  solve_type = PJFNK
  #petsc_options_iname = '-pc_type -pc_factor_mat_solver_package -pc_factor_shift_type -pc_factor_shift_amount'
  #petsc_options_value = 'lu       superlu_dist                  NONZERO               1e-15'
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_package'
  petsc_options_value = 'lu       superlu_dist'
  nl_abs_tol = 1e-8
  l_abs_tol = 1e-8
  dt = 1.0
  num_steps = 300
[]

[Outputs]
  exodus = true
  perf_graph = true
[]
