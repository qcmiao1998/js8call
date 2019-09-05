module js8b_module
    include 'js8/js8_params.f90'
    include 'js8/js8b_params.f90'

contains
    include 'js8/baselinejs8.f90'
    include 'js8/syncjs8.f90'
    include 'js8/js8_downsample.f90'
    include 'js8/syncjs8d.f90'
    include 'js8/genjs8refsig.f90'
    include 'js8/subtractjs8.f90'
    include 'js8/js8dec.f90'
end module js8b_module
