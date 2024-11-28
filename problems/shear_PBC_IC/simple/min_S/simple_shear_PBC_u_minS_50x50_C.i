!include /home/ruef/projects/simp/problems/PBC_files_to_include/PBC_50x50.i

nx = 50
ny = 50
p = 3
filter_radius = 1.5
E0 = 1
Emin = 1e-9
nu = 0.3
vol_frac = 0.5
c_limit = 0.05

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
  [C]
    family = MONOMIAL
    order = CONSTANT
  []
  [c]
    family = SCALAR
    order = FIRST
  []
  [dc]
    family = MONOMIAL
    order = CONSTANT
  []
  [ct]
    family = SCALAR
    order = FIRST
  []
  [dct]
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
  [copy_SE]
    type = MaterialRealAux
    property = strain_energy_density
    variable = C
    execute_on = TIMESTEP_END
  []
  [copy_compliance_sens]
    type = MaterialRealAux
    property = sensitivity
    variable = dc
    execute_on = TIMESTEP_END
  []
[]

[AuxScalarKernels]
  # [copy_C]
  #   type = PostprocessorScalarAux
  #   variable = c
  #   postprocessor = SE1
  #   execute_on = TIMESTEP_END
  # []
  [func_C]
    type = FunctionScalarAux
    variable = c
    function = func
    execute_on = TIMESTEP_END
  []
[]

[Functions]
  [func]
    type = ParsedFunction
    expression = 'c / ${c_limit} - 1'
    symbol_names = 'c'
    symbol_values = SE
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
    # inside = 1
    # outside = 0.1
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
    # inside = 1
    # outside = 0.1
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
    boundary = 'bl br'
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
  # [C]
  #   type = StrainEnergyDensity
  # []
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
  [interpolated_micro_vonmises_stress]
    order = CONSTANT
    family = MONOMIAL
  []
  [centroid_micro_vonmises_stress]
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
  # [two_u]
  #   type = ParsedAux
  #   variable = interpolated_vonmises_stress
  #   coupled_variables = 'rhoPhys micro_vonmises_stress'
  #   expression = 'rhoPhys^0.5 * micro_vonmises_stress'
  # []
[]

[UserObjects]
  [update]
    type = DensityUpdateMMA
    objective_function_sensitivity = dPM
    constraint_values = 'ct'
    constraint_sensitivities = 'dct'
    design_density = rho
    old_design_density1 = rho_old1
    old_design_density2 = rho_old2
    mma_lower_asymptotes = low
    mma_upper_asymptotes = upp
    move_limit = 0.01
  []
  # needs MaterialRealAux to copy sensitivity (mat prop) to dc aux variable
  [calc_sense]
    type = SensitivityFilterCustom
    sensitivities = 'dc dct dPM'
    mesh_generator = domain
    filter_type = density
    radius = ${filter_radius}
  []
  [stress_sens]
    type = StressResponseQpPNorm
    #type = StressResponseQpPMean
    usage = objective
    value = PM
    sensitivity = dPM
    stresses = 'micro_vonmises_stress micro_stress_xx micro_stress_xy micro_stress_yy'
    physical_density = rhoPhys
    E0 = ${E0}
    Emin = ${Emin}
    p = ${p}
    poissons_ratio = ${nu}
    mesh_generator = domain
    system_matrix = jacobian
    interpolated_micro_vonmises_stress = interpolated_micro_vonmises_stress
    micro_vonmises_stress = centroid_micro_vonmises_stress
    P = 12
    #scaling = true
  []
  [compliance_sens]
    type = ComplianceResponse
    usage = constraint
    limit = ${c_limit}
    value = ct
    sensitivity = dct
    physical_density = rhoPhys
    E0 = ${E0}
    Emin = ${Emin}
    p = ${p}
    poissons_ratio = ${nu}
    mesh_generator = domain
  []
  # [solid_stress]
  #   type = StressResponseAverageSolidStress
  #   usage = constraint
  #   value = asS
  #   sensitivity = dasS
  #   physical_density = rhoPhys
  #   E0 = ${E0}
  #   Emin = ${Emin}
  #   p = ${p}
  #   poissons_ratio = ${nu}
  #   mesh_generator = domain
  #   system_matrix = jacobian
  #   interpolated_micro_vonmises_stress = interpolated_micro_vonmises_stress
  #   micro_vonmises_stress = centroid_micro_vonmises_stress
  #   stress_response = stress_sens
  #   P = 16
  #   # not needed
  #   limit = 0
  #   stresses = 'micro_vonmises_stress micro_stress_xx micro_stress_xy micro_stress_yy'
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
  # [opt_conv]
  #   type = Terminator
  #   expression = 'change < 0.00125 & change != 0'
  #   execute_on = TIMESTEP_END
  #   execution_order_group = 5
  # []
[]

[Postprocessors]
  [change]
    type = VectorPostprocessorComponent
    vectorpostprocessor = max_abs_diff
    vector_name = Difference
    index = 0
    execute_on = 'initial timestep_end'
  []
  [SED]
    type = ElementIntegralMaterialProperty
    mat_prop = strain_energy_density
  []
  [SE]
    type = ParsedPostprocessor
    expression = '2 * SED'
    pp_names = SED
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
  num_steps = 500

  abort_on_solve_fail = true
[]

[Outputs]
  exodus = true
  perf_graph = true
[]

# [Debug]
#   show_material_props = true
#   show_execution_order = ALWAYS
# []
