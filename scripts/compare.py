import itertools
import netCDF4 as ex
import csv
import numpy as np

# open files
file1 = ex.Dataset(
    "problems/simple_shear_PBC_IC/mpi/simple_shear_2x2_moose_min_tol_out_serial.e"
)
file2 = ex.Dataset(
    "problems/simple_shear_PBC_IC/mpi/simple_shear_2x2_moose_min_tol_out_mpi.e"
)

# get names of nodal variables
encodedNodalVarNames = file1.variables["name_nod_var"]
nodalVarNames = []
for encName in encodedNodalVarNames:
    name = ""
    for char in encName:
        if not isinstance(char, np.ma.core.MaskedConstant):
            name += char.decode("UTF-8")
    nodalVarNames.append(name)

# get names of element variables in file1
encodedElementVarNames1 = file1.variables["name_elem_var"]
elementVarNames1 = []
for encName in encodedElementVarNames1:
    name = ""
    for char in encName:
        if not isinstance(char, np.ma.core.MaskedConstant):
            name += char.decode("UTF-8")
    elementVarNames1.append(name)

# get names of element variables in file 2
encodedElementVarNames2 = file2.variables["name_elem_var"]
elementVarNames2 = []
for encName in encodedElementVarNames2:
    name = ""
    for char in encName:
        if not isinstance(char, np.ma.core.MaskedConstant):
            name += char.decode("UTF-8")
    elementVarNames2.append(name)

# get values of nodal variables
checkNodalVars = ["disp_x", "disp_y"]
nodalVarIndex = []
nodalVars1 = []
nodalVars2 = []
for var in checkNodalVars:
    index = nodalVarNames.index(var) + 1
    # get values of desired variable
    timeStep = 1
    nodalVars1.append(file1.variables["vals_nod_var" + str(index)][timeStep].tolist())
    nodalVars2.append(file2.variables["vals_nod_var" + str(index)][timeStep].tolist())

# get values of element variables

checkElemVars = []

# checkElemVars = [
#     "rho",
#     "dc",
#     "cauchy_stress_xx",
#     "cauchy_stress_yy",
#     "cauchy_stress_xy",
#     "strain_xx",
#     "strain_yy",
#     "strain_xy",
# ]

elemVarIndex = []
elemVars1 = []
elemVars2 = []
for var in checkElemVars:
    var = var[:32]
    index1 = elementVarNames1.index(var) + 1
    index2 = index1
    # get values of desired variable
    timeStep = 1
    elemVars1.append(
        file1.variables["vals_elem_var" + str(index1) + "eb1"][timeStep].tolist()
    )
    elemVars2.append(
        file2.variables["vals_elem_var" + str(index2) + "eb1"][timeStep].tolist()
    )

file1.close()
file2.close()

if len(elemVars1) != len(elemVars2):
    raise Exception("Arrays have to be of same length")

# print(nodalVars1[0][1])
# print(nodalVars2[0][2599])

# print(elemVars1[0][6347])
# print(elemVars2[0][6347])

nodalAbsDiffs = [0] * len(nodalVars1)
isum = [0] * len(nodalVars1)
jsum = [0] * len(nodalVars1)
for var in range(len(nodalVars1)):
    for i, j in zip(nodalVars1[var], nodalVars2[var]):
        isum[var] += i
        jsum[var] += j
        nodalAbsDiffs[var] += abs(i - j)
print(list(zip(checkNodalVars, nodalAbsDiffs)))
print(isum, jsum)
print([i - j for i, j in zip(isum, jsum)])


elemAbsDiffs = [0] * len(elemVars1)
for var in range(len(elemVars1)):
    for i, j in zip(elemVars1[var], elemVars2[var]):
        elemAbsDiffs[var] += abs(i - j)
print(list(zip(checkElemVars, elemAbsDiffs)))

# print(list(zip(nodalVars1[var], nodalVars2[var])))
