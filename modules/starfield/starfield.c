#include <stdio.h>
#include <stdlib.h>
#include <cnovrparts.h>
#include <cnovrutil.h>
#include <cnovrmath.h>
#include <string.h>
#include <cnovrtcc.h>
#include <chew.h>
#include "flat_stars.h"
#include <cnovr.h>

cnovr_model * model;
cnovr_shader * shader;
cnovr_model * modelConst;
//cnovr_shader * shaderConst;

float * starfield_data1;	//ascention, declination, parallax [1]
float * starfield_data2;   //magnitude, bvcolor, vicolor [1]
flat_star * database;
int numstars = 0;
cnovr_pose * starpose;
int do_centering;

float galaxyalpha = 0;
int galaxyalphachange = 0;
cnovr_model * galaxyimage;
cnovr_pose  galaxyimagepose;
cnovr_shader * galaxyshader;
//typedef struct __attribute__((__packed__)) {
//uint32_t rascention_bams;
//int32_t declination_bams;
//int16_t parallax_10uas;
//uint16_t magnitude_mag1000;
//int16_t bvcolor_mag1000;
//int16_t vicolor_mag1000;
//} flat_star;

static void UpdateFunction( void * tag, void * opaquev )
{
	static double start;
	double now = OGGetAbsoluteTime();
	if( start < 1 ) start = now;

	int i;
	double framedt = now - start;
	start = now;

	if( do_centering )
	{
		//Get average point between two controllers and drag stars to that.
		cnovr_point3d runningpoint;

//cnovr_pose * CNOVRFocusGetTipPose( int device );
		add3d( runningpoint, CNOVRFocusGetTipPose(2)->Pos, CNOVRFocusGetTipPose(1)->Pos );
		scale3d( runningpoint, runningpoint, 0.5 );
		//Running point is now our target.
		sub3d( runningpoint, starpose->Pos, runningpoint );
		scale3d( runningpoint, runningpoint, 0.1 );
		sub3d( starpose->Pos, starpose->Pos, runningpoint );
	}

	galaxyalpha += galaxyalphachange * framedt;
	if( galaxyalpha < 0 ) galaxyalpha = 0;
	if( galaxyalpha > 1 ) galaxyalpha = 1;
}

static void RenderFunction( void * tag, void * opaquev )
{
	if( !model || !shader ) return;
	CNOVRRender( shader );
	if( starpose->Scale > 100000 ) starpose->Scale = 100000;
	if( starpose->Scale < .01 ) starpose->Scale = .01;
	float scales = starpose->Scale;
	float fvu[4] = { 1., scales, 0., 0. };
	//printf( "%f\n", scales );
	cnovr_pose outpose;
	memcpy( &outpose, starpose, sizeof( cnovr_pose ) );
	outpose.Scale = 1.0;
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
	glDepthFunc( GL_ALWAYS );
    glEnable(GL_DEPTH_CLAMP);

	glPointSize( 4 );
	glUniform4fv( 19, 1, fvu );
	CNOVRModelRenderWithPose( model, &outpose ); //Stars
	glLineWidth(.3);
	fvu[0] = 0;
	glUniform4fv( 19, 1, fvu );
	CNOVRModelRenderWithPose( modelConst, &outpose ); //Constellations

	//Galaxy image.
	if( galaxyalpha > 0.0 )
	{
		cnovr_pose galaxylocalpose;
		galaxylocalpose.Pos[0] = -5;
		galaxylocalpose.Pos[1] = -20;
		galaxylocalpose.Pos[2] = -40;

		galaxylocalpose.Rot[0] = 0.707;
		galaxylocalpose.Rot[1] = -.4;
		galaxylocalpose.Rot[2] = -0.707;
		galaxylocalpose.Rot[3] = 0;
		galaxylocalpose.Scale = 1.0;

		cnovr_pose outpose;
		apply_pose_to_pose( &outpose, starpose, &galaxylocalpose );
		outpose.Scale *= 100;
//		outpose.Pos[0] += .025;
//		outpose.Pos[1] -= .035;
		CNOVRRender( galaxyshader );
		fvu[0] = galaxyalpha;
		glUniform4fv( 19, 1, fvu );
		CNOVRModelRenderWithPose( galaxyimage, &outpose );
	}

	glDepthFunc( GL_LESS );
	glEnable( GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

///////////////////////////////////////////////////////////////////
cnovr_model_focus_controller focuscontrol;

void CollisionChecker( void * tag, void * opaquev )
{
	//(2)
	cnovrfocus_properties * p = (cnovrfocus_properties*)opaquev;
	//tag is tag
	//Use 'p' to figure out where the collision happened.
	CNOVRFocusRespond( focuscontrol.focusevent, 100.0 ); //We're just returning 100.0
}

int EventChecker( int event, cnovrfocus_capture * cap, cnovrfocus_properties * prop, int buttoninfo )
{
	//(3)
	void * tag = (void *)cap->opaque;

	if( event == CNOVRF_DRAG )
	{
		//Make effective pole length very short
		prop->capturedPassiveDistance = 0;	
		prop->NewPassiveRealDistance = 0;
	}

	CNOVRGeneralHandleFocusEvent( &focuscontrol, starpose, prop, event, buttoninfo );
	if( ( event == CNOVRF_DOWNNOFOCUS || event == CNOVRF_DOWNFOCUS ) )
	{
		if( buttoninfo == CTRLA_TRIGGER )
		{
			do_centering = 1;
		}
		if( buttoninfo == CTRLA_BUTTONB )
		{
			galaxyalphachange = (prop->devid==2)?1:-1;
		}
	}
	if( ( event == CNOVRF_UPNOFOCUS || event == CNOVRF_UPFOCUS ) )
	{
		if( buttoninfo == CTRLA_TRIGGER )
		{
			do_centering = 0;
		}
		if( buttoninfo == CTRLA_BUTTONB )
		{
			galaxyalphachange = 0;
		}
	}

	return 0;
}

cnovrfocus_capture focuseventdata = { 0, 0, 0, EventChecker };

void SetupFocusHandler()
{
	memset( &focuscontrol, 0, sizeof(focuscontrol) );
	//if( fc->focusevent ) CNOVRListDeleteTag( tag ); 
	focuseventdata.tcctag = GetTCCTag();
	focuscontrol.focusevent = &focuseventdata;
	CNOVRListAdd( cnovrLCollide, /*Tag*/0, CollisionChecker );  //(1)
}


//Temporary, probably going to add something like it to the engine.
static cnovr_model * make_genobj( int i, float gscalex, float gscaley )
{
	cnovr_model * ret = CNOVRModelCreate( 0, GL_TRIANGLES );
	cnovr_point4d extradata = { i, 0, 0, 0 };
	CNOVRModelAppendMesh( ret, 1, 1, 1, (cnovr_point3d){  gscalex, gscaley, 0 }, 0, &extradata );
	CNOVRModelAppendMesh( ret, 1, 1, 1, (cnovr_point3d){ -gscalex, gscaley, 0 }, 0, &extradata );
	return ret;
}

////////////////////////////////////////////////////////////////////

static void starfield_setup( void * tag, void * opaquev )
{
	shader = CNOVRShaderCreate( "starfield/starfield" );
	//shaderConst = CNOVRShaderCreate( "starfield/constellations" );
	CNOVRListAdd( cnovrLRender0, 0, RenderFunction );
	CNOVRListAdd( cnovrLUpdate, 0, UpdateFunction );
	int constellationlines = sizeof( constellationsegs ) / sizeof( constellationsegs[0] ) / 2;

	modelConst = CNOVRModelCreate( 0, GL_LINES );
	CNOVRModelSetNumVBOs( modelConst, 2 );
	modelConst->pGeos[0] = CNOVRCreateVBO( 4, 0, constellationlines*2, 0 );
	modelConst->pGeos[1] = CNOVRCreateVBO( 4, 0, constellationlines*2, 0 );
	int i;
	for( i = 0; i < constellationlines*2; i++ )
	{
		int starid = constellationsegs[i];
		memcpy( modelConst->pGeos[0]->pVertices + i*4, starfield_data1 + starid * 4, sizeof( float ) * 4 );
		memcpy( modelConst->pGeos[1]->pVertices + i*4, starfield_data2 + starid * 4, sizeof( float ) * 4 );
		//make the .w component allow the vertex shader to know if it's the beginning or end of the line.
		(modelConst->pGeos[0]->pVertices + i*4)[3] = i&1; 
		CNOVRModelTackIndex( modelConst, 1, i );		
		//printf( "%d %d\n", i, starid );
	}
	CNOVRVBOTaint( modelConst->pGeos[0] );
	CNOVRVBOTaint( modelConst->pGeos[1] );
	CNOVRModelTaintIndices( modelConst );

	model = CNOVRModelCreate( numstars, GL_POINTS );
	for( i = 0; i < numstars; i++ )
	{
		model->pIndices[i] = i;
	}
	CNOVRModelSetNumVBOs( model, 2 );
	model->pGeos[0] = CNOVRCreateVBO( 4, 0, numstars, 0 );
	model->pGeos[1] = CNOVRCreateVBO( 4, 0, numstars, 1 );
	memcpy( model->pGeos[0]->pVertices, starfield_data1, numstars * sizeof(float) * 4 );
	memcpy( model->pGeos[1]->pVertices, starfield_data2, numstars * sizeof(float) * 4 );
	CNOVRVBOTaint( model->pGeos[0] );
	CNOVRVBOTaint( model->pGeos[1] );
	CNOVRModelTaintIndices( model );

	galaxyshader = CNOVRShaderCreate( "starfield/galaxy" );
	galaxyimage = make_genobj( 0, 1, 1 );
	pose_make_identity( &galaxyimagepose );
	CNOVRModelApplyTextureFromFileAsync( galaxyimage, "starfield/Milky_Way_Galaxy.jpg" );

	SetupFocusHandler();
}

void CNOVRGenericSetInteractable( cnovr_model * m, void * tag, cnovr_model_focus_controller ** fcptr, cnovr_cb_fn collisioncheck, cnovrfocus_capture * focusevent )
{

}

void init()
{
}

void start( const char * identifier )
{
	printf( "Loading stars\n" );
	int len = 0;
	database = (flat_star*)CNOVRFileToString( "modules/starfield/tablize/flat_stars.dat", &len );
	numstars = len/sizeof(flat_star);
	printf( "Loaded %d bytes, %d stars\n", len, numstars );

	if( !database )
	{
		printf( "ERROR: Cannot load star database.\n" );
		return; //Don't proceed.
	}

	starfield_data1 = malloc( sizeof( float ) * 4 * numstars );
	starfield_data2 = malloc( sizeof( float ) * 4 * numstars );

	int i;
	for( i = 0; i < numstars; i++ )
	{
		flat_star * s = database + i;
		float * f1 = starfield_data1 + i * 4;
		float * f2 = starfield_data2 + i * 4;
		f1[0] = (((double)s->rascention_bams)/(1L<<32))*CNOVRPI*2; //to get radians
		f1[1] = (((double)s->declination_bams)/(1L<<31))*CNOVRPI;	//to get radians
		f1[2] = ((double)s->parallax_10uas)/100.0;	//to get mas
		f1[3] = 1;
		f2[0] = ((double)s->magnitude_mag1000)/1000.0;
		f2[1] = ((double)s->bvcolor_mag1000)/1000.0;
		f2[2] = ((double)s->vicolor_mag1000)/1000.0;
		f2[3] = 1;
	}

	printf( "Part 1 OK\n" );
	starpose = NAMEDPTRTYPED( "starpose", cnovr_pose );
	printf( "Starpose: %p\n", starpose );
	if( starpose->Scale == 0 )
	{
		pose_make_identity( starpose );
		starpose->Scale = 10000;
	}

	CNOVRJobTack( cnovrQPrerender, starfield_setup, 0, 0, 0 );
	//starfield_setup( 0, 0 );

	//CNOVRGenericSetInteractable( 0, cnovr_model_focus_controller ** fcptr, cnovr_cb_fn collisioncheck, cnovrfocus_capture * focusevent )
}

void stop( const char * identifier )
{
}

