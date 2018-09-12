double func2(double arg) {
    unsigned long int i = 1;
    unsigned long previous = 1;
    if(arg == 0) arg = 1024.0;
    double retval = arg;
    
    for(i=1; i<(arg*1000); i++) {
        retval *= previous;
        previous = i*arg;
    }
    
    return retval;
}

double funcX(double arg) {
    unsigned long int i = 1;
    unsigned long previous = 1;
    if(arg == 0) arg = 1024.0;
    double retval = arg;
    
    for(i=1; i<(arg*1000); i++) {
        retval *= previous;
        previous = i*arg;
    }
    
    return retval;
}
