project(CFE_ALTITUDE_APP C)

include_directories(fsw/mission_inc)
include_directories(fsw/platform_inc)

# Create the app module
add_cfe_app(altitude_app fsw/src/altitude_app.c)

# Include the public API from sensor_mpl3115a2
# to call library-provided functions
add_cfe_app_dependency(altitude_app sensor_mpl3115a2)
