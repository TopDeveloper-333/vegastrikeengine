#include "star.h"
#include <assert.h>
#include "vegastrike.h"
#include "vs_globals.h"
#include "gfx/camera.h"
#include "gfx/cockpit.h"
#include "config_xml.h"
#include "lin_time.h"
#include "galaxy_xml.h"
#if defined(__APPLE__) || defined(MACOSX)
    #include <OpenGL/gl.h>
#else
    #include <GL/gl.h>
#endif
//#include "cmd/unit.h"
#define SINX 1
#define SINY 2
#define SINZ 4

unsigned int NumStarsInGalaxy () {
	unsigned int count=0;
	map<std::string,GalaxyXML::Galaxy>::iterator i = _Universe->getGalaxy()->getHeirarchy().begin();
	map<std::string,GalaxyXML::Galaxy>::iterator e = _Universe->getGalaxy()->getHeirarchy().end();
	for (;i!=e;++i) {
		count+=(*i).second.getHeirarchy().size();
	}
	return count;
}
class StarIter {
	map<std::string,GalaxyXML::Galaxy>::iterator sector;
	map<std::string,GalaxyXML::Galaxy>::iterator system;
public:
	StarIter() {
		sector = _Universe->getGalaxy()->getHeirarchy().begin();
		if (!Done()) {
			system = (*sector).second.getHeirarchy().begin();
			if (system == (*sector).second.getHeirarchy().end()){
				++(*this);
			}
		}
	}
	void operator ++ () {
		if (!Done()) {
			if (system!=(*sector).second.getHeirarchy().end()) {
				++system;
			}
		}
		while (!Done()&&system==(*sector).second.getHeirarchy().end()) {
			++sector;
			if (!Done()) {
				system  = (*sector).second.getHeirarchy().begin();
			}
		}
	}
	bool Done() const{
		return (sector==_Universe->getGalaxy()->getHeirarchy().end());
	}
	GalaxyXML::Galaxy *Get() {
		if (!Done()) {
			return &(*system).second;
		}else {
			return NULL;
		}
	}
	std::string GetSystem() {
		if (!Done())
			return (*system).first;
		else
			return "Nowhere";
	}
	std::string GetSector() {
		if (!Done()){
			return (*sector).first;
		}else{
			return "NoSector";
		}
	}
};
//bool shouldwedraw
static void saturate(float &r, float &g, float &b) {
	static float conemin = XMLSupport::parse_float(vs_config->getVariable("graphics","starmincolorval",".3"));
	static float colorpower = XMLSupport::parse_float(vs_config->getVariable("graphics","starcolorpower",".25"));
	if (r<conemin)
		r+=conemin;
	if (g<conemin)
		g+=conemin;
	if (b<conemin)
		b+=conemin;
	r = pow(r,colorpower);
	g = pow(g,colorpower);
	b = pow(b,colorpower);	
}
bool computeStarColor (float &r, float &g, float &b, Vector luminmax, float distance, float maxdistance){
	saturate(r,g,b);
	static float luminscale = XMLSupport::parse_float(vs_config->getVariable("graphics","starluminscale",".001"));
	static float starcoloraverage = XMLSupport::parse_float(vs_config->getVariable("graphics","starcoloraverage",".6"));
	static float starcolorincrement= XMLSupport::parse_float(vs_config->getVariable("graphics","starcolorincrement","100"));
	float dissqr = distance*distance/(maxdistance*maxdistance);
	float lum = 100*luminmax.i/(luminmax.k*dissqr);
	lum = log((double)luminmax.i*10./(double)luminmax.j)*luminscale/dissqr;
//	fprintf (stderr,"luminmax %f lumnow %f\n",luminmax.i/(luminmax.k*dissqr),lum);
	float clamp=starcoloraverage+lum/starcolorincrement;
	if (clamp>1)
		clamp=1;
	if (lum>clamp)
		lum=clamp;
	r*=lum;
	g*=lum;
	b*=lum;
	static float starcolorcutoff = XMLSupport::parse_float(vs_config->getVariable("graphics","starcolorcutoff",".1"));
	return lum>starcolorcutoff;
}
namespace StarSystemGent {
extern GFXColor getStarColorFromRadius(float radius);
}
StarVlist::StarVlist (int num ,float spread,const std::string &our_system_name) {
	lasttime=0;
	_Universe->AccessCamera()->GetPQR(newcamr,camq,camr);
	newcamr=camr;
	newcamq=camq;
	
	this->spread=spread;
	static float staroverlap = XMLSupport::parse_float(vs_config->getVariable("graphics","star_overlap","1"));
	float xyzspread = spread*2*staroverlap;
	if (!our_system_name.empty())
		num =NumStarsInGalaxy();
	
	GFXColorVertex * tmpvertex = new GFXColorVertex[num*2];
	memset (tmpvertex,0,sizeof(GFXVertex)*num*2);
	StarIter si;
	int starcount=0;
	int j=0;
	float xcent=0;
	float ycent=0;
	float zcent=0;
	Vector starmin(0,0,0);
	Vector starmax(0,0,0);
	float minlumin=1;
	float maxlumin=1;
	if (our_system_name.size()>0) {
		sscanf (_Universe->getGalaxyProperty(our_system_name,"xyz").c_str(),
				"%f %f %f",
				&xcent,
				&ycent,
				&zcent);
		
		for (StarIter i;!i.Done();++i) {
			float xx,yy,zz;
			if (3==sscanf((*i.Get())["xyz"].c_str(),"%f %f %f",&xx,&yy,&zz)) {
				xx-=xcent;
				yy-=ycent;
				zz-=zcent;
				if (xx<starmin.i)
					starmin.i=xx;
				if (yy<starmin.j)
					starmin.j=yy;
				if (zz<starmin.k)
					starmin.k=zz;
				if (xx>starmax.i)
					starmax.i=xx;
				if (yy>starmax.j)
					starmax.j=yy;
				if (zz>starmax.k)
					starmax.k=zz;
				float lumin;
				if (1==sscanf((*i.Get())["luminosity"].c_str(),"%f",&lumin)) {
					if (lumin>maxlumin) {
						maxlumin=lumin;
					}
					if (lumin<minlumin)
						minlumin=lumin;
				}
			}
		}
	}
	float maxdistance = starmax.Magnitude();
	float mindistance = starmin.Magnitude();
	if (maxdistance<mindistance)
		maxdistance=mindistance;
	fprintf (stderr,"Min (%f, %f, %f) Max(%f, %f, %f) MinLumin %f, MaxLumin %f",
			 starmin.i,starmin.j,starmin.k,starmax.i,starmax.j,starmax.k,minlumin,maxlumin);
			 
	for (int y=0;y<num;++y) {
		
		tmpvertex[j+1].x = -.5*xyzspread+rand()*((float)xyzspread/RAND_MAX);
		tmpvertex[j+1].y = -.5*xyzspread+rand()*((float)xyzspread/RAND_MAX);
		tmpvertex[j+1].z = -.5*xyzspread+rand()*((float)xyzspread/RAND_MAX);
		tmpvertex[j+1].r=1;
		tmpvertex[j+1].g=1;
		tmpvertex[j+1].b=1;
		
		tmpvertex[j+1].i=.57735;
		tmpvertex[j+1].j=.57735;
		tmpvertex[j+1].k=.57735;
		int incj=2;
		if (our_system_name.size()>0&&!si.Done()) {

			starcount++;
			float xorig,yorig,zorig;
			if (3==sscanf ((*si.Get())["xyz"].c_str(),
						   "%f %f %f",
						   &xorig,
						   &yorig,
						   &zorig)) {
				if (xcent!=xorig) {				
					tmpvertex[j+1].x=xorig-xcent;
				}
				if (ycent!=yorig) {
					tmpvertex[j+1].y=yorig-ycent;
				}
				if (zcent!=zorig) {
					tmpvertex[j+1].z=zorig-zcent;
				}
			}
			std::string radstr=(*si.Get())["sun_radius"];
			if (radstr.size()){
				float rad = XMLSupport::parse_float(radstr);
				GFXColor suncolor (StarSystemGent::getStarColorFromRadius(rad));
				tmpvertex[j+1].r=suncolor.r;
				tmpvertex[j+1].g=suncolor.g;
				tmpvertex[j+1].b=suncolor.b;
			}
			float lumin=1;
			sscanf((*si.Get())["luminosity"].c_str(),"%f",&lumin);
			
			float distance = Vector(tmpvertex[j+1].x,
									tmpvertex[j+1].y,
									tmpvertex[j+1].z).Magnitude();
			if (!computeStarColor(tmpvertex[j+1].r,
								  tmpvertex[j+1].g,
								  tmpvertex[j+1].b,
								  Vector(lumin,minlumin,maxlumin),
								  distance,maxdistance)) {
				incj=0;
			}
			++si;
		}
		
   		tmpvertex[j].i=tmpvertex[j+1].i;
		tmpvertex[j].j=tmpvertex[j+1].j;
		tmpvertex[j].k=tmpvertex[j+1].k;
		tmpvertex[j].x =tmpvertex[j+1].x;//+spread*.01;
		tmpvertex[j].y =tmpvertex[j+1].y;//;+spread*.01;
		tmpvertex[j].z =tmpvertex[j+1].z;		
		tmpvertex[j].r=0;
		tmpvertex[j].g=0;
		tmpvertex[j].b=0;
		j+=incj;
	}
	fprintf (stderr,"Read In Star Count %d used: %d\n",starcount,j/2);
	vlist= new GFXVertexList (GFXLINE,j,tmpvertex, j, true,0);  
	delete []tmpvertex;
}
void StarVlist::UpdateGraphics() {
	double time = getNewTime();
	if (time!=lasttime) {
		camr= newcamr;
		camq=newcamq;
		Vector newcamp;
		_Universe->AccessCamera()->GetPQR (newcamp,newcamq,newcamr);
		lasttime=time;
	}
}
void StarVlist::BeginDrawState (const QVector &center, const Vector & velocity, const Vector & torque, bool roll, bool yawpitch) {
	UpdateGraphics();
	Matrix rollMatrix;
	static float velstreakscale= XMLSupport::parse_float (vs_config->getVariable ("graphics","velocity_star_streak_scale","5"));

	Vector vel (-velocity*velstreakscale);
	GFXColorMaterial(AMBIENT|DIFFUSE);
	GFXColorVertex * v = vlist->BeginMutate(0)->colors;
	int numvertices = vlist->GetNumVertices();

	static float torquestreakscale= XMLSupport::parse_float (vs_config->getVariable ("graphics","torque_star_streak_scale","1"));
	for (int i=0;i<numvertices-1;i+=2) {
		Vector vpoint (v[i+1].x,v[i+1].y,v[i+1].z);
		Vector recenter =(vpoint-center.Cast());
		if (roll) {
			RotateAxisAngle(rollMatrix,torque,torque.Magnitude()*torquestreakscale*.003);			
			vpoint = Transform(rollMatrix,recenter)+center.Cast();
		}
		v[i].x=vpoint.i-vel.i;
		v[i].y=vpoint.j-vel.j;
		v[i].z=vpoint.k-vel.k;
	}
	vlist->EndMutate();
	vlist->LoadDrawState();
	vlist->BeginDrawState();
}
void StarVlist::Draw() {
	vlist->Draw();
	vlist->Draw(GFXPOINT,vlist->GetNumVertices());
}
void StarVlist::EndDrawState() {
	vlist->EndDrawState();
	GFXColorMaterial(0);
}
Stars::Stars(int num, float spread): vlist((num/STARnumvlist)+1,spread,""),spread(spread){

  int curnum = num/STARnumvlist+1;
  fade = blend=true;
  ResetPosition(QVector(0,0,0));
}
void Stars::SetBlend(bool blendit, bool fadeit) {
	blend = true;//blendit;
	fade = true;//fadeit;
}
void Stars::Draw() {
  const QVector cp (_Universe->AccessCamera()->GetPosition());
  UpdatePosition(cp);
  //  GFXLightContextAmbient(GFXColor(0,0,0,1));
  GFXColor (1,1,1,1);
  GFXLoadIdentity(MODEL);
  GFXEnable(DEPTHWRITE);
  GFXDisable (TEXTURE0);
  GFXDisable (TEXTURE1);
  GFXEnable (DEPTHTEST);
  if (blend) {
	GFXBlendMode (ONE,ONE);
  } else {
	GFXBlendMode (ONE,ZERO);
  }
  int ligh;
  GFXSelectMaterial (0);
  if (fade) {
    static float star_spread_attenuation = XMLSupport::parse_float(vs_config->getVariable("graphics","star_spread_attenuation",".2"));
    GFXLight FadeLight (true, GFXColor (cp.i,cp.j,cp.k), GFXColor (0,0,0,1), GFXColor (0,0,0,1), GFXColor (1,1,1,1), GFXColor (.01,0,1/(star_spread_attenuation*star_spread_attenuation*spread*spread)));
    GFXCreateLight (ligh,FadeLight,true);
    GFXEnable (LIGHTING);
  } else {
    GFXDisable (LIGHTING);
  }
  
  vlist.BeginDrawState(_Universe->AccessCamera()->GetR().Scale(-spread).Cast(),_Universe->AccessCamera()->GetVelocity(),_Universe->AccessCamera()->GetAngularVelocity(),false,false);
  _Universe->AccessCamera()->UpdateGFX(GFXFALSE,GFXFALSE,GFXFALSE);
	
  for (int i=0;i<STARnumvlist;i++) {
    if (i>=1)
      GFXTranslateModel (pos[i]-pos[i-1]);
    else
      GFXTranslateModel (pos[i]);
    vlist.Draw();
  }
  vlist.EndDrawState();
_Universe->AccessCamera()->UpdateGFX(GFXTRUE,GFXFALSE,GFXFALSE)	  ;

  GFXEnable (TEXTURE0);
  GFXEnable (TEXTURE1);
  if (fade)
    GFXDeleteLight (ligh);
  GFXLoadIdentity(MODEL);
}
static void upd (double &a, double &b, double &c, double &d, double &e, double &f, double &g, double &h, double &i, const double cp, const float spread) {
  //  assert (a==b&&b==c&&c==d&&d==e&&e==f);	
  if (a!=b||a!=c||a!=d||a!=e||a!=f||!FINITE (a)) {
    a=b=c=d=e=f=0;
  }
  while (a-cp > 1.5*spread) {
    a-=3*spread;
    b-=3*spread;
    c-=3*spread;
    d-=3*spread;
    e-=3*spread;
    f-=3*spread;
    g-=3*spread;
    h-=3*spread;
    i-=3*spread;
  }
  while (a-cp < -1.5*spread) {
    a+=3*spread;
    b+=3*spread;
    c+=3*spread;
    d+=3*spread;
    e+=3*spread;
    f+=3*spread;
    g+=3*spread;
    h+=3*spread;
    i+=3*spread;
  }
}

void Stars::ResetPosition (const QVector &cent){
  for (int i=0;i<3;i++) {
    for (int j=0;j<3;j++) {
      for (int k=0;k<3;k++) {
	pos[i*9+j*3+k].Set ((i-1)*spread,(j-1)*spread,(k-1)*spread);
	pos[i*9+j*3+k]+=cent;
      }
    }
  }
}
void Stars::UpdatePosition(const QVector & cp) {

  if (fabs(pos[0].i-cp.i)>3*spread||fabs(pos[0].j-cp.j)>3*spread||fabs(pos[0].k-cp.k)>3*spread) {
    ResetPosition(cp);
    return;
  }
  upd (pos[0].i,pos[1].i,pos[2].i, pos[3].i,pos[4].i,pos[5].i,pos[6].i, pos[7].i,pos[8].i, cp.i, spread);
  upd (pos[9].i,pos[10].i,pos[11].i, pos[12].i,pos[13].i,pos[14].i,pos[15].i, pos[16].i,pos[17].i, cp.i, spread);
  upd (pos[18].i,pos[19].i,pos[20].i, pos[21].i,pos[22].i,pos[23].i,pos[24].i, pos[25].i,pos[26].i, cp.i, spread);

  upd (pos[0].j,pos[1].j,pos[2].j, pos[9].j,pos[10].j,pos[11].j,pos[18].j, pos[19].j,pos[20].j, cp.j, spread);
  upd (pos[3].j,pos[4].j,pos[5].j, pos[12].j,pos[13].j,pos[14].j,pos[21].j, pos[22].j,pos[23].j, cp.j, spread);
  upd (pos[6].j,pos[7].j,pos[8].j, pos[15].j,pos[16].j,pos[17].j,pos[24].j, pos[25].j,pos[26].j, cp.j, spread);

  upd (pos[0].k,pos[3].k,pos[6].k, pos[9].k,pos[12].k,pos[15].k,pos[18].k, pos[21].k,pos[24].k, cp.k, spread);
  upd (pos[1].k,pos[4].k,pos[7].k, pos[10].k,pos[13].k,pos[16].k,pos[19].k, pos[22].k,pos[25].k, cp.k, spread);
  upd (pos[2].k,pos[5].k,pos[8].k, pos[11].k,pos[14].k,pos[17].k,pos[20].k, pos[23].k,pos[26].k, cp.k, spread);

}

StarVlist::~StarVlist () {
  delete vlist;

}
