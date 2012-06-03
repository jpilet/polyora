## Julien Pilet <julien.pilet@aptarism.com>, 2012

# Reads the output of "qtpolyora -rt"
function [X, t] = load_tracks(filename, patch_size)
    num_pixels = patch_size * patch_size;
    f = fopen(filename, "r");
    if (nargout >= 2)
	t = fread(f, Inf, "long=>long", num_pixels * 4);
    endif
    fseek(f, 8, SEEK_SET);
    X = fread(f, [num_pixels, Inf], sprintf("%d*float=>double", num_pixels), 8);
    fclose(f);

endfunction
