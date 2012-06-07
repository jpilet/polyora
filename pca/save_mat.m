function save_mat(M, name, fd) 
    fprintf(fd, "%s: !!opencv-matrix\n", name);
    fprintf(fd, "rows: %d\ncols: %d\n", size(M));
    fprintf(fd, "dt: f\ndata: [");
    V = M(:);
    fprintf(fd, " %f,", V(1:size(V)-1));
    fprintf(fd, " %f ]\n", V(size(V,1)));
