!include /home/ruef/projects/simp/problems/PBC_files_to_include/PBC_100x100.i

nx = 100
ny = 100
p = 3
vol_frac = 0.5
filter_radius = 1.5
E0 = 1
Emin = 1e-9
nu = 0.3

[Problem]
  extra_tag_matrices = jacobian
[]

[GlobalParams]
  displacements = 'disp_x disp_y'
[]

[Mesh]
  [domain]
    type = GeneratedMeshGenerator
    dim = 2
    xmin = -5
    xmax = 5
    ymin = -5
    ymax = 5
    nx = ${nx}
    ny = ${ny}
  []
  [top_right]
    type = ExtraNodesetGenerator
    input = domain
    new_boundary = tr
    coord = '5 5 0'
  []
  [bottom_right]
    type = ExtraNodesetGenerator
    input = top_right
    new_boundary = br
    coord = '5 -5 0'
  []
  [top_left]
    type = ExtraNodesetGenerator
    input = bottom_right
    new_boundary = tl
    coord = '-5 5 0'
  []
  [bottom_left]
    type = ExtraNodesetGenerator
    input = top_left
    new_boundary = bl
    coord = '-5 -5 0'
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
  [PM]
    family = SCALAR
    order = FIRST
  []
  [dPM]
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
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
[]

[Kernels]
  [stress_x]
    type = StressDivergenceTensors
    component = 0
    variable = disp_x
    extra_matrix_tags = jacobian
  []
  [stress_y]
    type = StressDivergenceTensors
    component = 1
    variable = disp_y
    extra_matrix_tags = jacobian
  []
[]

[ICs]
  [rho_IC]
    type = BoundingBoxIC
    variable = rho
    x1 = -0.75
    x2 = 0.751
    y1 = -0.75
    y2 = 0.751
    inside = 1e-9
    outside = ${vol_frac}
  []
  [rho_phys_IC]
    type = BoundingBoxIC
    variable = rhoPhys
    x1 = -0.75
    x2 = 0.751
    y1 = -0.75
    y2 = 0.751
    inside = 1e-9
    outside = ${vol_frac}
  []
  [rho_old1_IC]
    type = BoundingBoxIC
    variable = rho_old1
    x1 = -0.75
    x2 = 0.751
    y1 = -0.75
    y2 = 0.751
    inside = 1e-9
    outside = ${vol_frac}
  []
  [rho_old2_IC]
    type = BoundingBoxIC
    variable = rho_old2
    x1 = -0.75
    x2 = 0.751
    y1 = -0.75
    y2 = 0.751
    inside = 1e-9
    outside = ${vol_frac}
  []
[]

[BCs]
  [no_y]
    type = DirichletBC
    variable = disp_y
    boundary = 'bl br tl'
    value = 0
  []
  [no_x]
    type = DirichletBC
    variable = disp_x
    boundary = 'bl'
    value = 0
  []
  [disp]
    type = DirichletBC
    variable = disp_x
    boundary = 'tl'
    value = 1
  []
[]

[Materials]
  [elasticity_tensor]
    type = ComputeVariableIsotropicElasticityTensorPlaneStress
    youngs_modulus = E_phys
    poissons_ratio = ${nu}
    args = 'Emin rhoPhys p E0'
  []
  [E_phys]
    type = DerivativeParsedMaterial
    # Emin + (density^penal) * (E0 - Emin)
    expression = '${Emin} + (rhoPhys ^ ${p}) * (${E0}-${Emin})'
    coupled_variables = 'rhoPhys'
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
  [compute_strain]
    type = ComputeSmallStrain
  []
  [elasticity_tensor_0]
    type = ComputeIsotropicElasticityTensorPlaneStress
    base_name = micro
    youngs_modulus = ${E0}
    poissons_ratio = ${nu}
  []
  [stress_micro]
    type = ComputeLinearElasticStress
    base_name = micro
  []
  [strain_micro]
    type = ComputeSmallStrain
    base_name = micro
  []
[]

[AuxVariables]
  [vonmises]
    order = CONSTANT
    family = MONOMIAL
  []
  [micro_stress_xx]
    order = CONSTANT
    family = MONOMIAL
  []
  [micro_stress_yy]
    order = CONSTANT
    family = MONOMIAL
  []
  [micro_stress_xy]
    order = CONSTANT
    family = MONOMIAL
  []
  [micro_vonmises_stress]
    order = CONSTANT
    family = MONOMIAL
  []
[]

[AuxKernels]
  [vonmises]
    type = RankTwoScalarAux
    rank_two_tensor = stress
    variable = vonmises
    scalar_type = VonMisesStress
  []
  [micro_stress_xx]
    type = RankTwoAux
    rank_two_tensor = micro_stress
    variable = micro_stress_xx
    index_i = 0
    index_j = 0
  []
  [micro_stress_yy]
    type = RankTwoAux
    rank_two_tensor = micro_stress
    variable = micro_stress_yy
    index_i = 1
    index_j = 1
  []
  [micro_stress_xy]
    type = RankTwoAux
    rank_two_tensor = micro_stress
    variable = micro_stress_xy
    index_i = 0
    index_j = 1
  []
  [micro_vonmises_stress]
    type = RankTwoScalarAux
    rank_two_tensor = micro_stress
    variable = micro_vonmises_stress
    scalar_type = VonMisesStress
  []
[]

[UserObjects]
  [update]
    type = DensityUpdateMMA
    objective_function_sensitivity = dc
    constraint_values = 'V'
    constraint_sensitivities = 'dV'
    design_density = rho
    old_design_density1 = rho_old1
    old_design_density2 = rho_old2
    mma_lower_asymptotes = low
    mma_upper_asymptotes = upp
    move_limit = 0.05
  []
  # needs MaterialRealAux to copy sensitivity (mat prop) to dc aux variable
  [calc_sense]
    type = SensitivityFilterCustom
    sensitivities = 'dc dV'
    mesh_generator = domain
    filter_type = density
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
  # [stress_sens]
  #   type = StressResponseQpPNorm
  #   usage = constraint
  #   limit = 0.05
  #   value = PM
  #   sensitivity = dPM
  #   stresses = 'micro_vonmises_stress micro_stress_xx micro_stress_xy micro_stress_yy'
  #   physical_density = rhoPhys
  #   E0 = ${E0}
  #   Emin = ${Emin}
  #   p = ${p}
  #   poissons_ratio = ${nu}
  #   mesh_generator = domain
  #   system_matrix = jacobian
  # []
  # [stress_sens]
  #   type = StressResponseVerbartPMean
  #   usage = constraint
  #   limit = 0.03
  #   value = PM
  #   sensitivity = dPM
  #   stresses = 'micro_vonmises_stress micro_stress_xx micro_stress_xy micro_stress_yy'
  #   physical_density = rhoPhys
  #   E0 = ${E0}
  #   Emin = ${Emin}
  #   p = ${p}
  #   poissons_ratio = ${nu}
  #   mesh_generator = domain
  # []
  [heaviside]
    type = DensityFilter
    design_density = rho
    physical_density = rhoPhys
    mesh_generator = domain
    radius = ${filter_radius}
  []
  [opt_conv]
    type = Terminator
    expression = 'change < 0.0025 & change != 0'
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
  active = 'superlu'
  [superlu]
    type = SMP
    full = true
    petsc_options = '-snes_ksp_ew'
    petsc_options_iname = '-pc_type -pc_factor_mat_solver_package -pc_factor_shift_type -pc_factor_shift_amount'
    petsc_options_value = 'lu       superlu_dist                  NONZERO               1e-15'
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON

  nl_abs_tol = 1e-6
  l_abs_tol = 1e-6
  dt = 1.0
  num_steps = 300

  abort_on_solve_fail = true
[]

[Outputs]
  exodus = true
  perf_graph = true
[]
