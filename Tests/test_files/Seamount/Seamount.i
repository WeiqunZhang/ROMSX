# ------------------  INPUTS TO MAIN PROGRAM  -------------------
max_step = 10
stop_time = 30000.0

amrex.fpe_trap_invalid = 1

fabarray.mfiter_tile_size = 1024 1024 1024

# PROBLEM SIZE & GEOMETRY
# dims come from ROMS ana_grid, and must match vals in prob.cpp right now
geometry.prob_lo     =   0.       0.  -5000.
geometry.prob_hi     =   320000.  320000.       0.

amr.n_cell           =  49   48   13

# periodic in x to match WRF setup
geometry.is_periodic = 1 1 0
#ylo.type = "SlipWall"
#yhi.type = "SlipWall"
zlo.type = "SlipWall"
zhi.type = "SlipWall"

# TIME STEP CONTROL
remora.fixed_dt       = 60.0 # Timestep size (seconds)
# NDTFAST  = 30.0 # Number of baratropic steps => 300.0/30.0 = 10.0
#remora.fixed_fast_dt  = 10.0 # Baratropic timestep size (seconds)
# remora.fixed_fast_dt  = 300.0 # Baratropic timestep size (seconds) testing value
remora.fixed_ndtfast_ratio  = 20 # Baratropic timestep size (seconds)

remora.flat_bathymetry=0

# DIAGNOSTICS & VERBOSITY
remora.sum_interval   = 1       # timesteps between computing mass
remora.v              = 0       # verbosity in REMORA.cpp (0: none, 1: print boxes, etc, 2: print values)
amr.v              = 1       # verbosity in Amr.cpp

# REFINEMENT / REGRIDDING
amr.max_level       = 0       # maximum level number allowed

# CHECKPOINT FILES
remora.check_file      = chk        # root name of checkpoint file
remora.check_int       = -57600      # number of timesteps between checkpoints

# PLOTFILES
remora.plot_file_1     = plt        # prefix of plotfile name
remora.plot_int_1      = 1          # number of timesteps between plotfiles
remora.plot_vars_1     = salt temp x_velocity y_velocity z_velocity
remora.plotfile_type   = amrex

# SOLVER CHOICE
remora.use_coriolis = true
remora.horizontal_advection_scheme = "upstream3" # upstream3 or centered4
remora.spatial_order = 2

remora.coriolis_f0 = 1.0e-4 #-8.26e-5

remora.theta_s = 6.5
remora.theta_b = 2.0
remora.tcline = 100.

# PROBLEM PARAMETERS (optional)
remora.R0    = 1027.0  # background density value (Kg/m3) used in Linear Equation of State
remora.S0    = 32.0    # background salinity (nondimensional) constant
remora.T0    = 10.0    # background potential temperature (Celsius) constant
remora.Tcoef = 1.7e-4  # linear equation of state parameter (1/Celcius)
remora.Scoef = 0.0     # linear equation of state parameter (nondimensional)
remora.rho0  = 1025.0  # Mean density (Kg/m3) used when Boussinesq approx is inferred
