function [V, avg, s] = pca(samples)
    centered = center(samples);
    [U, S, V] = svd(centered' * centered);
    if (nargout > 1)
	avg = sum(samples) / size(samples, 1);
    endif
    if (nargout > 2)
	s = diag(S);
    endif
endfunction
