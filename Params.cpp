/**********************************
 * FILE NAME: Params.cpp
 *
 * DESCRIPTION: Definition of Parameter class (Revised 2020)
 **********************************/

#include "Params.h"

/**
 * Constructor
 */
Params::Params(): PORTNUM(8001) {}

/**
 * FUNCTION NAME: setparams
 *
 * DESCRIPTION: Set the parameters for this test case
 */
void Params::setparams(char *config_file) {
	//trace.funcEntry("Params::setparams");
	char CRUD[10];
	FILE *fp = fopen(config_file,"r");

	fscanf(fp,"MAX_NNB: %d", &MAX_NNB);
	fscanf(fp,"\nSINGLE_FAILURE: %d", &SINGLE_FAILURE);
	fscanf(fp,"\nDROP_MSG: %d", &DROP_MSG);
	fscanf(fp,"\nMSG_DROP_PROB: %lf", &MSG_DROP_PROB);
	fscanf(fp,"\nCRUD_TEST: %s", CRUD);

	// Not part of this assignment.
	// auto type = testTypeMap.find(CRUD);
	// if(type == testTypeMap.end()){
	// 	throw std::runtime_error("Unavailable Test Type!");
	// }
	// else{
	// 	this->CRUDTEST = type->second;
	// }

	//printf("Parameters of the test case: %d %d %d %lf\n", MAX_NNB, SINGLE_FAILURE, DROP_MSG, MSG_DROP_PROB);

	EN_GPSZ = MAX_NNB;
	STEP_RATE=.25;
	MAX_MSG_SIZE = 4000;
	globaltime = 0;
	dropmsg = 0;
	allNodesJoined = 0;
	for ( int i = 0; i < EN_GPSZ; i++ ) {
		allNodesJoined += i;
	}
	fclose(fp);
	//trace.funcExit("Params::setparams", SUCCESS);
	return;
}

/**
 * FUNCTION NAME: getcurrtime
 *
 * DESCRIPTION: Return time since start of program, in time units.
 * 				For a 'real' implementation, this return time would be the UTC time.
 */
int Params::getcurrtime(){
    return globaltime;
}
