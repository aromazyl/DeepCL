// Copyright Hugh Perkins 2014 hughperkins at gmail
//
// This Source Code Form is subject to the terms of the Mozilla Public License, 
// v. 2.0. If a copy of the MPL was not distributed with this file, You can 
// obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#define VIRTUAL virtual
#define STATIC static

class OpenCLHelper;
class CLWrapper;

class PoolingPropagate {
public:
    OpenCLHelper *cl;

    const int numPlanes;
    const int inputBoardSize;
    const int poolingSize;

    // [[[cog
    // import cog_addheaders
    // cog_addheaders.add()
    // ]]]
    // classname: PoolingPropagate
    // cppfile: PoolingPropagate.cpp

    PoolingPropagate( OpenCLHelper *cl, int numPlanes, int inputBoardSize, int poolingSize );
    STATIC PoolingPropagate *instance( OpenCLHelper *cl, int numPlanes, int inputBoardSize, int poolingSize );
    STATIC PoolingPropagate *instanceForTest( OpenCLHelper *cl, int numPlanes, int inputBoardSize, int poolingSize );
    STATIC PoolingPropagate *instanceSpecific( int idx, OpenCLHelper *cl, int numPlanes, int inputBoardSize, int poolingSize );
    VIRTUAL void propagate( CLWrapper *inputData, CLWrapper *outputData );
    VIRTUAL float *propagate( float *input );

    // [[[end]]]
};
