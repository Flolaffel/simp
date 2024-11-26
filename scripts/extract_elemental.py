import itertools
import netCDF4 as ex
import csv
import numpy as np
import os

# open file
file = ex.Dataset(
    "problems/shear_PBC_IC/simple/stress_cons/simple_shear_u_PBC_50x50_V50_QpPN0-06_m0-01_P100.e"
)

# get names of element variables
encodedElementVarNames = file.variables["name_elem_var"]
elementVarNames = []
for encName in encodedElementVarNames:
    name = ""
    for char in encName:
        if not isinstance(char, np.ma.core.MaskedConstant):
            name += char.decode("UTF-8")
    elementVarNames.append(name)
# print(elementVarNames)


# get values of element variables
timeSteps = [91, 92]

elemVarIndex = []
elemVars = []
for i, var in enumerate(checkElemVars):
    var = var[:32]
    index = elementVarNames.index(var) + 1
    # get values of desired variable
    timeStep = timeSteps[i]
    elemVars.append(
        file.variables["vals_elem_var" + str(index) + "eb1"][timeStep].tolist()
    )

file.close()

script_path = os.path.dirname(os.path.realpath(__file__))
with open(script_path + "/extract_elemental.csv", "w") as csvfile:
    writer = csv.writer(csvfile)
    writer.writerows(zip(*elemVars))
