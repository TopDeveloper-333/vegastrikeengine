#include "gl_light.h"
#include <queue>
#include <list>
#include <vector>
using std::priority_queue;
#include "hashtable_3d.h"
using std::list;
using std::vector;
 //optimization globals
float intensity_cutoff=.05;//something that would normally round down
float optintense=.2;
float optsat = .95;


struct light_key {
  int number;
  float intensity_key;
  light_key () {
    intensity_key = number=0;
  }
  light_key (int num, float inte) {
    number = num;
    intensity_key = inte;
  }
};
static bool operator < (light_key tmp1,light_key tmp2) {return tmp1.intensity_key<tmp2.intensity_key;}

static priority_queue<light_key> lightQ;

static list <int> pickedlights [2];
static list <int>* newpicked=&pickedlights[0];
static list <int>* oldpicked=&pickedlights[1];

inline int getIndex (const LineCollide & t) {
    return t.object.i;
}
static void swappicked () {
    if (newpicked==&pickedlights[0]) {
	newpicked=&pickedlights[1];
	oldpicked=&pickedlights[0];
    }else {
	newpicked=&pickedlights[0];
	oldpicked=&pickedlights[1];
    }
    newpicked->clear();
}

void unpicklights () {
	for (std::list <int>::iterator i=newpicked->begin();i!=newpicked->end();i++) {
    if (GLLights[*i].index==-1 || GLLights[(*_llights)[*i].Target()].index!=*i)
      continue;//a lengthy operation... Since picked lights may have been smashed    
    int targ =(*_llights)[*i].Target(); 
    if (GLLights[targ].options&OpenGLL::GL_ENABLED) {
      glDisable (GL_LIGHT0+targ);
      GLLights[targ].options=OpenGLL::GLL_LOCAL;
      GLLights[targ].index=-1;
      (*_llights)[*i].Target() =-1;//unref
    }

  }
  newpicked->clear();
}

static inline bool picklight (const LineCollide& light, const Vector & center, const float rad, const int lightsenabled);
typedef vector <LineCollideStar> veclinecol;
void GFXPickLights (const Vector & center, const float radius) {
    Vector tmp;
    void * rndvar = (void *)rand();
    int sizeget=2;
    int lightsenabled = _GLLightsEnabled;
    LineCollide tmpcollide;
    tmp = Vector(radius,radius,radius);
    tmpcollide.Mini = center-tmp;
    tmpcollide.Maxi = center+tmp;
    tmpcollide.hhuge=false;//fixme!! may well be hhuge...don't have enough room in tmppickt
    tmpcollide.object.i=0;//FIXME, should this be -1?
    tmpcollide.type=LineCollide::UNIT;
    swappicked();
    veclinecol *tmppickt[HUGEOBJECT+1];
    //FIXMESPEEDHACK    if (radius < CTACC) {
	lighttable.Get (center, tmppickt);
	//FIXMESPEEDHACK} else {
	//FIXMESPEEDHACKsizeget = lighttable.Get (&tmpcollide, tmppickt); 
	//FIXMESPEEDHACK}
    for (int j=0;j<sizeget;j++) {
	  veclinecol::iterator i;
      for (i=tmppickt[j]->begin();i!=tmppickt[j]->end();i++){
	//warning::duplicates may Exist
	//FIXMESPEEDHACKif (i->lc->lastchecked!=rndvar) {
	//FIXMESPEEDHACKi->lc->lastchecked = rndvar;
	  if (picklight (*i->lc,center,radius,lightsenabled)) {
	    newpicked->push_back (i->GetIndex());
	    lightsenabled++;
	  }
	  //FIXMESPEEDHACK}
      }
    }
    gfx_light::dopickenables ();  
}

static inline bool picklight (const LineCollide& light, const Vector & center, const float rad, const int lightsenabled) {
  return true;

}

void gfx_light::dopickenables () {
  newpicked->sort();//sort it to find minimum num lights changed from last time.
  std::list<int>::iterator traverse= newpicked->begin();
  std::list<int>::iterator oldtrav;
  while (traverse!=newpicked->end()&&(!oldpicked->empty())) {
    oldtrav = oldpicked->begin();
    while (oldtrav!=oldpicked->end()&& *oldtrav < *traverse) {
      oldtrav++;
    }
    if (((*traverse)==(*oldtrav))&&((*_llights)[*oldtrav].target>=0)) {
      //BOGUS ASSERT... just like this light wasn't on if it was somehow clobberedassert (GLLights[(*_llights)[oldpicked->front()].target].index == oldpicked->front());
      oldpicked->erase (oldtrav);//already taken care of. main screen turn on ;-)
    } 
    traverse++;
  }
  oldtrav = oldpicked->begin();
  while (oldtrav!=oldpicked->end()) {
    if (GLLights[(*_llights)[(*oldtrav)].target].index != (*oldtrav))
      continue;//don't clobber what's not yours
    GLLights[(*_llights)[(*oldtrav)].target].index = -1;
    GLLights[(*_llights)[(*oldtrav)].target].options &= (OpenGLL::GL_ENABLED&OpenGLL::GLL_LOCAL);//set it to be desirable to kill
    oldtrav++;
  }
  traverse= newpicked->begin();
  while (traverse!=newpicked->end()) {
    if ((*_llights)[*traverse].target==-1) {
	int gltarg = findLocalClobberable();
	if (gltarg==-1) {
	    newpicked->erase (traverse,newpicked->end());//erase everything on the picked list. Nothing can fit;
	    break;
	}
	(*_llights)[(*traverse)].ClobberGLLight(gltarg);
    }
    traverse++;
  }    
  
  while (!oldpicked->empty()) {
    int glind=(*_llights)[(*oldtrav)].target;
    if ((GLLights[glind].options&OpenGLL::GL_ENABLED)&&GLLights[glind].index==-1) {//if hasn't been duly clobbered
      glDisable (GL_LIGHT0+glind);
      GLLights[glind].options &= (~OpenGLL::GL_ENABLED);
    }
    (*_llights)[(*oldtrav)].target=-1;//make sure it doesn't think it owns any gl lights!
    oldpicked->pop_front();
  }

}

#ifdef MIRACLESAVESDAY

void /*GFXDRVAPI*/ GFXPickLights (const float * transform) {
  //picks 1-5 lights to use
  // glMatrixMode(GL_MODELVIEW);
  //glPushMatrix();
  //float tm [16]={1,0,0,1000,0,1,0,1000,0,0,1,1000,0,0,0,1};
  //glLoadIdentity();
  //  GFXLoadIdentity(MODEL);
  if (!GFXLIGHTING) return ;
  Vector loc (transform[12],transform[13],transform[14]);
  SetLocalCompare (loc);
  newQsize=0;
  unsigned int i;
  light_key tmpvar;
  unsigned int lightQsize = lightQ.size();
  for (i=0;i<lightQsize;i++) {
    tmpvar = lightQ.top();
    if (i<GFX_MAX_LIGHTS) {
      //do more stuff with intensity before hacking it to hell
      if (i>GFX_OPTIMAL_LIGHTS) {
	if ((tmpvar.intensity_key-optintense)*(GFX_MAX_LIGHTS-GFX_OPTIMAL_LIGHTS) < (i - GFX_OPTIMAL_LIGHTS)*(optsat-optintense))
	  break;//no new lights
      }else {
	if (i==GFX_OPTIMAL_LIGHTS-2&&tmpvar.intensity_key<.25*optintense)  
	  break;//no new lights
	if (i==GFX_OPTIMAL_LIGHTS-1&&tmpvar.intensity_key<.5*optintense)
	  break;//no new lights
	if (i==GFX_OPTIMAL_LIGHTS&&tmpvar.intensity_key < optintense)
	  break;
      }
      newQsize++;
#ifdef PER_MESH_ATTENUATE
      if ((*_llights)[i].vect[3]) {
	//intensity now has become the attenuation factor
	newQ[i]= tmpvar.number;
	AttenuateQ[i]=tmpvar.intensity_key/(*_llights)[i].intensity;
      }else {
#endif
	newQ[i]= tmpvar.number;
#ifdef PER_MESH_ATTENUATE
	AttenuateQ[i]=0;
      }
#endif
    } else {
      break;
    }
    lightQ.pop();
  }
  unsigned int light;
  for (i=0;i<newQsize;i++) {
    light = newQ[i];
    if ((*_llights)[light].target>=0) {
#ifdef PER_MESH_ATTENUATE
      if (AttenuateQ[i]) 
	EnableExistingAttenuated (light,AttenuateQ[i]);
      else
#endif
	EnableExisting (newQ[i]);
      /***daniel 081901
      AttTmp[0]=(*_llights)[light].vect[0]-loc.i;
      AttTmp[1]=(*_llights)[light].vect[1]-loc.j;
      AttTmp[2]=(*_llights)[light].vect[2]-loc.k;
      VecT[3]=0;
      VecT[0]=AttTmp[0]*transform[0]+AttTmp[1]*transform[1]+AttTmp[2]*transform[2];
      VecT[1]=AttTmp[0]*transform[4]+AttTmp[1]*transform[5]+AttTmp[2]*transform[6];
      VecT[2]=AttTmp[0]*transform[8]+AttTmp[1]*transform[9]+AttTmp[2]*transform[10];
      
      glLightfv (GL_LIGHT0+(*_llights)[light].target, GL_POSITION, VecT);
      ****/
    }
  }
  for (i=0;i<GFX_MAX_LIGHTS;i++) {
    DisableExisting(i);
  }

  // go through newQ and tag all existing lights for enablement
  // disable rest of lights
  // ForceActivate the rest.
  unsigned int tmp=0;//where to start looking for disabled lights
  unsigned int newtarg=0;
  for (i=0;i<newQsize;i++) {
    light = newQ[i];
    if ((*_llights)[light].target==-1) {
      if (AttenuateQ[i]) 
	ForceEnableAttenuated (light,AttenuateQ[i],tmp,newtarg);
      else
	ForceEnable (light,tmp,newtarg); 
      AttTmp[0]=(*_llights)[light].vect[0]-loc.i;
      AttTmp[1]=(*_llights)[light].vect[1]-loc.j;
      AttTmp[2]=(*_llights)[light].vect[2]-loc.k;
      AttTmp[3]=0;
      VecT[0]=AttTmp[0]*transform[0]+AttTmp[1]*transform[1]+AttTmp[2]*transform[2];
      VecT[1]=AttTmp[0]*transform[4]+AttTmp[1]*transform[5]+AttTmp[2]*transform[6];
      VecT[2]=AttTmp[0]*transform[8]+AttTmp[1]*transform[9]+AttTmp[2]*transform[10];
      glLightfv (GL_LIGHT0+(*_llights)[light].target, GL_POSITION, VecT);

      //      glLightfv (GL_LIGHT0+(*llight_dat)[newQ[i]].target, GL_POSITION, /*wrong*/(*_llights)[light].vect);
    }
  }
  //glPopMatrix();
}

/*
      curlightloc.intensity=(curlight.specular[0]+curlight.specular[1]+curlight.specular[2]+curlight.diffuse[0]+curlight.diffuse[1]+curlight.diffuse[2]+curlight.ambient[0]+curlight.ambient[1]+curlight.ambient[2])*.333333333333333333333333;//calculate new cuttof val for light 'caching'
*/
#endif


