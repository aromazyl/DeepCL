#include <iostream>
#include <random>

#include "NeuralNet.h"
#include "BackpropErrors.h"
#include "ActivationFunction.h"

#include "gtest/gtest.h"

#include "test/gtest_supp.h"
#include "test/Sampler.h"
#include "test/WeightRandomizer.h"

using namespace std;

// This file contains tests for calculating errors for the upstream layer, whcih is currently a bit slow 
// as at the time of first creating this file :-P

TEST( testbackproperrors, checknumerically ) {
    float learningRate = 0.01f;
    const int batchSize = 1;
    const int boardSize = 1;

    NeuralNet *net = NeuralNet::maker()->planes(1)->boardSize(boardSize)->instance();
    net->convolutionalMaker()->numFilters(1)->filterSize(1)->biased(0)->tanh()->insert();
    net->convolutionalMaker()->numFilters(1)->filterSize(1)->biased(0)->tanh()->insert();
    net->setBatchSize( batchSize );

    int inputSize = net->layers[0]->getResultsSize();
    int resultsSize = net->layers[1]->getResultsSize();
    int weightsSize = net->layers[1]->getWeightsSize();

    float *inputData = new float[max(10000, inputSize )];
    float *expectedResults = new float[max(10000, resultsSize )];
    memset( inputData, 0, sizeof(float) * max(10000, inputSize ) );
    memset( expectedResults, 0, sizeof(float) * max(10000, resultsSize ) );
    int seed = 0;
    std::mt19937 random = WeightRandomizer::randomize( inputData, max(10000, inputSize ), -1.0f, 1.0f );
    WeightRandomizer::randomize( random, expectedResults, max(10000, resultsSize ), -1.0f, 1.0f );
    WeightRandomizer::randomize( random, net->layers[1]->weights, weightsSize, -0.1f, 0.1f );
    dynamic_cast<ConvolutionalLayer*>(net->layers[1])->weightsWrapper->copyToDevice();
//    for( int i = 0; i < inputSize; i++ ) {
//        cout << "inputData[" << i << "]=" << inputData[i] << endl;
//    }
//    for( int i = 0; i < resultsSize; i++ ) {
//        cout << "expectedResults[" << i << "]=" << expectedResults[i] << endl;
//    }

    float *weightsBefore = new float[weightsSize];
    float *currentWeights = net->layers[1]->weights;
    for( int i = 0; i < weightsSize; i++ ) {
        weightsBefore[i] = currentWeights[i];
    }

//    net->print();
    cout << "propagate" <<endl;
    net->propagate( inputData );
//    net->print();
    float loss = net->calcLoss(expectedResults);
    float losslayer1 = net->layers[1]->calcLoss(expectedResults);
    cout << "losslayer1 " << losslayer1 << endl;

    cout << "backprop now" <<endl;
    net->backProp( learningRate, expectedResults );
//    net->layers[1]->print();
    net->propagate( inputData );
//    net->layers[1]->print();
    float loss2 = net->calcLoss(expectedResults);
    float lossChange = loss - loss2;
    cout << " loss " << loss << " loss2 " << loss2 << " change: " << lossChange << endl;

    float *newWeights = net->layers[1]->weights;
    float sumWeightDiff = 0;
    float sumWeightDiffSquared = 0;
    for( int i = 0; i < weightsSize; i++ ) {
        float diff = newWeights[i] - weightsBefore[i];
        sumWeightDiff += diff;
        sumWeightDiffSquared += diff * diff;
    }
    cout << "sumweightsdiff " << sumWeightDiff << endl;
//    cout << "sumweightsdiff / learningrate " << (sumWeightDiff / learningRate ) << endl;
//    cout << "sum weightsdiffsquared " << (sumWeightDiffSquared/ learningRate / learningRate * boardSize ) << endl;

    float estimatedLossChangeFromW = sumWeightDiffSquared/ learningRate * sqrt(boardSize) * batchSize;

    cout << " loss change " << lossChange << " estimatedLossChangeFromW " << estimatedLossChangeFromW << endl;
    cout << abs(estimatedLossChangeFromW - lossChange ) / lossChange << endl;    
    cout << abs(estimatedLossChangeFromW - lossChange ) / estimatedLossChangeFromW << endl;    
    EXPECT_GT( 0.01f * boardSize * boardSize, abs(estimatedLossChangeFromW - lossChange ) / lossChange ); 
    EXPECT_GT( 0.01f * boardSize * boardSize, abs(estimatedLossChangeFromW - lossChange ) / estimatedLossChangeFromW ); 

//    delete[] weights1;
//    delete[] errors;
//    delete[] results;
    delete[] inputData;
}

float *test( int boardSize ) {
    const int batchSize = 128;
    LayerDimensions dim;
    dim.setInputPlanes( 32 ).setInputBoardSize( 28 ).setNumFilters( 32 ).setFilterSize( 5 )
        .setBiased( true ).setPadZeros( true );

    int weightsSize = dim.filtersSize;
    int biasWeightsSize = dim.numFilters;
    int resultsSize = batchSize * dim.outputCubeSize;
    float *weights = new float[max(10000, weightsSize ) ];
    float *biasWeights = new float[max( 10000, biasWeightsSize)];
    float *errors = new float[max(10000, resultsSize )];
    float *results = new float[max(10000, resultsSize )];
    WeightRandomizer::randomize( weights, max(10000, weightsSize ), -1, 1 );
    WeightRandomizer::randomize( biasWeights, max( 10000, biasWeightsSize), -1, 1 );
    WeightRandomizer::randomize( errors, max(10000, resultsSize ), -1, 1 );
    WeightRandomizer::randomize( results, max(10000, resultsSize ), -1, 1 );

    OpenCLHelper cl;
    BackpropErrors *backpropErrorsImpl = BackpropErrors::instanceForTest( &cl, dim, new ReluActivation() );
    Timer timer;
    float *errorsForUpstream = backpropErrorsImpl->backpropErrors( batchSize, results, weights, biasWeights, errors );
    StatefulTimer::dump(true);
    timer.timeCheck("after calcing errors");

    Sampler::printSamples( "errorsForUpstream", batchSize * dim.inputCubeSize, errorsForUpstream );

    delete backpropErrorsImpl;

    delete[] errors;
    delete[] weights;
    delete[] biasWeights;

    return errorsForUpstream;
}

// we want to test calcerrors for layer 2 in a network like:
//    NeuralNet *net = NeuralNet::maker()->planes(1)->boardSize(28)->instance();
//    net->convolutionalMaker()->numFilters(32)->filterSize(5)->relu()->biased()->insert();
//    net->convolutionalMaker()->numFilters(32)->filterSize(5)->relu()->biased()->insert();
//    net->convolutionalMaker()->numFilters(10)->filterSize(20)->tanh()->biased(config.biased)->insert();
TEST( testbackproperrors, board28 ) {
    float *errorsForUpstream = test(28);
    EXPECT_FLOAT_NEAR( -1.66007, errorsForUpstream[68268] );
    EXPECT_FLOAT_NEAR( 0.823709, errorsForUpstream[2927151] );
    EXPECT_FLOAT_NEAR( 6.99365, errorsForUpstream[1746549] );
    EXPECT_FLOAT_NEAR( 7.25249, errorsForUpstream[576704] );
    EXPECT_FLOAT_NEAR( 7.88787, errorsForUpstream[570179] );
    delete[] errorsForUpstream;
}

TEST( testbackproperrors, board19 ) { // make it work for a board19 first :-)
    float *errorsForUpstream = test(19);
    EXPECT_FLOAT_NEAR( -24.5602, errorsForUpstream[158380] );
    EXPECT_FLOAT_NEAR( 7.39012, errorsForUpstream[2607] );
    EXPECT_FLOAT_NEAR( -6.50315, errorsForUpstream[546421] );
    EXPECT_FLOAT_NEAR( -1.22025, errorsForUpstream[429248] );
    EXPECT_FLOAT_NEAR( -8.89935, errorsForUpstream[1200963] );
    delete[] errorsForUpstream;

//    const int batchSize = 128;
//    LayerDimensions dim;
//    dim.setInputPlanes( 32 ).setInputBoardSize( 19 ).setNumFilters( 32 ).setFilterSize( 5 )
//        .setBiased( true ).setPadZeros( true );    const int batchSize = 128;
//    LayerDimensions dim;
//    dim.setInputPlanes( 32 ).setInputBoardSize( 28 ).setNumFilters( 32 ).setFilterSize( 5 )
//        .setBiased( true ).setPadZeros( true );

//    int weightsSize = dim.filtersSize;
//    int biasWeightsSize = dim.numFilters;
//    int resultsSize = batchSize * dim.outputCubeSize;
//    float *weights = new float[max(10000, weightsSize ) ];
//    float *biasWeights = new float[max( 10000, biasWeightsSize)];
//    float *errors = new float[max(10000, resultsSize )];
//    float *results = new float[max(10000, resultsSize )];
//    WeightRandomizer::randomize( weights, max(10000, weightsSize ), -1, 1 );
//    WeightRandomizer::randomize( biasWeights, max( 10000, biasWeightsSize), -1, 1 );
//    WeightRandomizer::randomize( errors, max(10000, resultsSize ), -1, 1 );
//    WeightRandomizer::randomize( results, max(10000, resultsSize ), -1, 1 );

//    OpenCLHelper cl;
//    BackpropErrors *backpropErrorsImpl = BackpropErrors::instanceForTest( &cl, dim, new ReluActivation() );
//    Timer timer;
//    float *errorsForUpstream = backpropErrorsImpl->backpropErrors( batchSize, results, weights, biasWeights, errors );
//    StatefulTimer::dump(true);
//    timer.timeCheck("after calcing errors");

//    Sampler::printSamples( "errorsForUpstream", batchSize * dim.inputCubeSize, errorsForUpstream );

//    EXPECT_FLOAT_NEAR( -1.66007, errorsForUpstream[68268] );
//    EXPECT_FLOAT_NEAR( 0.823709, errorsForUpstream[2927151] );
//    EXPECT_FLOAT_NEAR( 6.99365, errorsForUpstream[1746549] );
//    EXPECT_FLOAT_NEAR( 7.25249, errorsForUpstream[576704] );
//    EXPECT_FLOAT_NEAR( 7.88787, errorsForUpstream[570179] );

//    delete backpropErrorsImpl;

//    delete[] errorsForUpstream;
//    delete[] errors;
//    delete[] weights;
//    delete[] biasWeights;


//    int weightsSize = dim.filtersSize;
//    int biasWeightsSize = dim.numFilters;
//    int resultsSize = batchSize * dim.outputCubeSize;
//    float *weights = new float[max(10000, weightsSize ) ];
//    float *biasWeights = new float[max( 10000, biasWeightsSize)];
//    float *errors = new float[max(10000, resultsSize )];
//    float *results = new float[max(10000, resultsSize )];
//    WeightRandomizer::randomize( weights, max(10000, weightsSize ), -1, 1 );
//    WeightRandomizer::randomize( biasWeights, max( 10000, biasWeightsSize), -1, 1 );
//    WeightRandomizer::randomize( errors, max(10000, resultsSize ), -1, 1 );
//    WeightRandomizer::randomize( results, max(10000, resultsSize ), -1, 1 );

//    OpenCLHelper cl;
//    BackpropErrors *backpropErrorsImpl = BackpropErrors::instanceForTest( &cl, dim, new ReluActivation() );
//    Timer timer;
//    float *errorsForUpstream = backpropErrorsImpl->backpropErrors( batchSize, results, weights, biasWeights, errors );
//    StatefulTimer::dump(true);
//    timer.timeCheck("after calcing errors");

//    Sampler::printSamples( "errorsForUpstream", batchSize * dim.inputCubeSize, errorsForUpstream );

//    EXPECT_FLOAT_NEAR( -24.5602, errorsForUpstream[158380] );
//    EXPECT_FLOAT_NEAR( 7.39012, errorsForUpstream[2607] );
//    EXPECT_FLOAT_NEAR( -6.50315, errorsForUpstream[546421] );
//    EXPECT_FLOAT_NEAR( -1.22025, errorsForUpstream[429248] );
//    EXPECT_FLOAT_NEAR( -8.89935, errorsForUpstream[1200963] );

//    delete backpropErrorsImpl;

//    delete[] errorsForUpstream;
//    delete[] errors;
//    delete[] weights;
//    delete[] biasWeights;
}

TEST( testbackproperrors, comparespecific ) {
    const int batchSize = 5;
    LayerDimensions dim;
    dim.setInputPlanes( 1 ).setInputBoardSize( 5 ).setNumFilters( 1 ).setFilterSize( 5 )
        .setBiased( true ).setPadZeros( false );

    int weightsSize = dim.filtersSize;
    int biasWeightsSize = dim.numFilters;
    int resultsSize = batchSize * dim.outputCubeSize;
    float *weights = new float[max(10000, weightsSize ) ];
    float *biasWeights = new float[max( 10000, biasWeightsSize)];
    float *errors = new float[max(10000, resultsSize )];
    float *results = new float[max(10000, resultsSize )];
    memset( weights, 0, sizeof(float) * max(10000, weightsSize ) );
    memset( biasWeights, 0, sizeof(float) * max(10000, biasWeightsSize ) );
    memset( errors, 0, sizeof(float) * max(10000, resultsSize ) );
    memset( results, 0, sizeof(float) * max(10000, resultsSize ) );
//    WeightRandomizer::randomize( weights, max(10000, weightsSize ), -1, 1 );
//    WeightRandomizer::randomize( biasWeights, max( 10000, biasWeightsSize), -1, 1 );
//    WeightRandomizer::randomize( errors, max(10000, resultsSize ), -1, 1 );
    WeightRandomizer::randomizeInts( weights, max(10000, weightsSize ), 1, 3 );
//    WeightRandomizer::randomizeInts( biasWeights, max( 10000, biasWeightsSize), 0, 3 );
    WeightRandomizer::randomizeInts( errors, max(10000, resultsSize ), 0, 3 );
    WeightRandomizer::randomizeInts( results, max(10000, resultsSize ), 0, 3 );

//    weights[0] = 3;
//    weights[1] = 5;
//    weights[2] = 4;

//    weights[25] = 4;
//    weights[49] = 4;

//    weights[50] = 4;
//    weights[99] = 4;

//    weights[75] = 4;
//    weights[99] = 4;

//    weights[100] = 3;
//    weights[124] = 3;

//    errors[0] = 2;
//    errors[1] = 7;
//    errors[2] = 3;
//    errors[3] = 1;
//    errors[4] = 8;
//    errors[5] = 6;

    OpenCLHelper cl;
    BackpropErrors *backpropErrorsImpl1 = BackpropErrors::instanceSpecific( 1, &cl, dim, new ReluActivation() );
    float *errorsForUpstream1 = backpropErrorsImpl1->backpropErrors( batchSize, results, weights, biasWeights, errors );
    BackpropErrors *backpropErrorsImpl2 = BackpropErrors::instanceSpecific( 2, &cl, dim, new ReluActivation() );
    float *errorsForUpstream2 = backpropErrorsImpl2->backpropErrors( batchSize, results, weights, biasWeights, errors );

    int errorsForUpstreamSize = batchSize * dim.inputCubeSize;
    cout << dim << endl;
    for( int i = 0; i < 25; i++ ) {
        cout << "results[" << i << "]=" << errorsForUpstream1[i] << " " << errorsForUpstream2[i];
        if( i < resultsSize ) {
            if( errorsForUpstream1[i] == errorsForUpstream2[i] ) {
                cout << " SAME";
            } else {
                cout << " DIFF";
            }
        } else {
            cout << "     ";
        }
        cout << "  || " << errorsForUpstream2[100+i] ;
        cout << "  || " << errorsForUpstream2[200+i] ;
        cout << "  || " << errorsForUpstream2[300+i] ;
        cout << "  || " << errorsForUpstream2[400+i] ;
        cout << "  || " << errorsForUpstream2[500+i] ;
        cout << "  || " << errorsForUpstream2[600+i] ;
        cout << "  || " << errorsForUpstream2[700+i] << endl;
    }
    bool same = true;
    int errCount = 0;
    for( int i = 0; i < errorsForUpstreamSize; i++ ) {
        if( errorsForUpstream1[i] != errorsForUpstream2[i] ) {
            cout << "DIFF: i " << i << " " << errorsForUpstream1[i] << " != " << errorsForUpstream2[i] << endl;
            same = false;
            errCount++;
            if( errCount == 5 ) {
                cout << " ... " << endl;
                break;
            }
        }
    }
    EXPECT_EQ( true, same );

    delete backpropErrorsImpl1;
    delete backpropErrorsImpl2;

    delete[] errorsForUpstream1;
    delete[] errorsForUpstream2;
    delete[] errors;
    delete[] weights;
    delete[] biasWeights;
}


