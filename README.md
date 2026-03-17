A quick msgpack-c wrapper for MATLAB & Octave.

~two orders of magnitude faster than https://github.com/bastibe/matlab-msgpack

Generally follows jsondecode convention for deserializing (heterogeneous) arrays.

Known issues:

 - Strong assumption that maps only have strings as keys. We could do containers.Map to be more flexible, but they're more uncommon?
