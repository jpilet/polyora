function projected = project(patches, V, avg)
    projected = (patches * V)  - repmat(avg, size(patches, 1), 1) * V;
endfunction
