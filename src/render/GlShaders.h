#pragma once

#define BLOCKY_SHADOW_GLSL R"(
uniform int uOccCount;
uniform vec3 uOccCenter[8];
uniform vec3 uOccHalf[8];
uniform mat3 uOccRot[8];
uniform int uSunShadow;   // 1 = ombre proiettate da sole/luna (default 0 = off)
uniform float uNight;     // 0 = pieno giorno, 1 = piena notte (default 0)
// 1.0 se un box interseca il raggio entro maxT
float rayBlocked(vec3 P,vec3 dir,float maxT){
  int nOcc=min(uOccCount,8);
  if(nOcc<=0) return 0.0;
  for(int i=0;i<nOcc;++i){
    vec3 o=uOccRot[i]*(P-uOccCenter[i]);
    vec3 d=uOccRot[i]*dir;
    // evita divisione per zero sui raggi paralleli a una faccia
    d=vec3(abs(d.x)<1e-5?1e-5:d.x, abs(d.y)<1e-5?1e-5:d.y, abs(d.z)<1e-5?1e-5:d.z);
    vec3 h=uOccHalf[i];
    vec3 t0=(-h-o)/d;
    vec3 t1=( h-o)/d;
    vec3 tmin=min(t0,t1);
    vec3 tmax=max(t0,t1);
    float tN=max(max(tmin.x,tmin.y),tmin.z);
    float tF=min(min(tmax.x,tmax.y),tmax.z);
    // tN>bias: il fragment non e' sulla superficie del box → niente auto-ombra
    if(tF>tN && tN>0.06 && tN<maxT-0.06) return 1.0;
  }
  return 0.0;
}
float lightVisibility(vec3 P,vec3 Lp){
  vec3 seg=Lp-P;
  float segLen=length(seg);
  if(segLen<1e-3) return 1.0;
  return 1.0-rayBlocked(P,seg/segLen,segLen);
}
// —— Ombra di sole/luna: shadow map, quindi con la forma vera della mesh ——
uniform sampler2DShadow uShadowMap;
uniform mat4 uLightVP;
uniform float uShadowTexel;
float skyLightVisibility(vec3 P,vec3 N,vec3 toSky){
  if(uSunShadow==0) return 1.0;
  // Offset piccolo: l'acne la toglie il polygon offset del depth pass.
  // Se qui si esagera, l'ombra si stacca dalla base della mesh.
  vec4 lp=uLightVP*vec4(P+N*0.04,1.0);
  vec3 pr=lp.xyz/lp.w*0.5+0.5;
  if(pr.z>1.0||pr.x<0.0||pr.x>1.0||pr.y<0.0||pr.y>1.0) return 1.0;
  float bias=0.0006;
  float s=0.0;
  for(int y=-1;y<=1;++y)
    for(int x=-1;x<=1;++x)
      s+=texture(uShadowMap,vec3(pr.xy+vec2(x,y)*uShadowTexel,pr.z-bias));
  return s/9.0;
}
// Illuminazione direzionale con ora del giorno + ombra portata.
// Riempimento ambientale alto e sole morbido = look "default Roblox":
// luminoso, ombre soffici (i lati/le ombre non crollano nel nero).
uniform vec3 uSunColor;    // tinta reale del sole (calda ad alba/tramonto, bianca a mezzogiorno, blu di notte)
uniform vec3 uAmbientTint; // fill ambientale: Ambient/OutdoorAmbient del Lighting Service
uniform vec3 uSkyZenith;   // colore del cielo allo zenit in questo istante (stesso gradiente di SunSky)
uniform vec3 uSkyHorizon;  // colore del cielo all'orizzonte in questo istante
// AO di contatto: riusa la stessa lista di box vicini gia' raccolta per le
// ombre finte dei punti luce (uOccCenter/uOccHalf/uOccRot) — nessun dato
// nuovo da calcolare in CPU. Scurisce solo il fill ambientale (mai la luce
// diretta) quando il fragment e' vicino alla superficie di un'altra part,
// per dare peso/contatto alle giunture senza un vero G-buffer AO.
uniform int uAoOccStart; // indici < questo sono box-corpo player/remoti: esclusi dall'AO (autoscurimento)
float contactAO(vec3 P){
  int nOcc=min(uOccCount,8);
  if(nOcc<=0) return 1.0;
  float occ=0.0;
  const float r0=2.0;
  for(int i=uAoOccStart;i<nOcc;++i){
    vec3 local=uOccRot[i]*(P-uOccCenter[i]);
    vec3 h=uOccHalf[i];
    vec3 clamped=clamp(local,-h,h);
    float dist=length(local-clamped);
    if(dist>0.08 && dist<r0)
      occ+=(1.0-dist/r0);
  }
  return clamp(1.0-occ*0.35,0.35,1.0);
}
// Foschia in distanza: uFogColor/Start/End esistono nel Lighting Service e
// nel pannello Studio da tempo ma nessuno shader li leggeva ancora — la
// scena non aveva mai foschia reale, solo lo slider morto.
uniform vec3 uFogColor;
uniform float uFogStart,uFogEnd;
vec3 applyFog(vec3 rgb,vec3 P,vec3 camPos){
  float d=length(P-camPos);
  float t=clamp((d-uFogStart)/max(uFogEnd-uFogStart,1.0),0.0,1.0);
  t=t*t*(3.0-2.0*t);
  return mix(rgb,uFogColor,t*0.9);
}
// Riflesso finto stile Roblox 2021: nessuna vera scena riflessa, solo il
// gradiente di cielo campionato lungo la direzione riflessa. Zero texture
// aggiuntive, riusa i colori di cielo gia' calcolati una volta a frame.
vec3 fakeSkyReflection(vec3 R){
  float h=clamp(R.y*0.5+0.5,0.0,1.0);
  return mix(uSkyHorizon,uSkyZenith,pow(h,0.85));
}
// Quanto un materiale mostra il riflesso ambientale, dedotto da
// specular/roughness gia' esistenti (Metal/Glass -> forte, Plastic/Grass
// -> quasi nullo): nessun nuovo parametro materiale da mantenere.
// Il colore base resta sempre presente (envAmt tetto a 0.85) cosi' il
// materiale non diventa mai uno specchio puro, solo "riflesso finto".
vec3 applyEnvReflection(vec3 base,vec3 N,vec3 V,float specular,float roughness){
  vec3 R=reflect(-V,N);
  float NdotV=clamp(dot(N,V),0.0,1.0);
  float fresnel=pow(1.0-NdotV,5.0);
  float envAmt=clamp(specular*(1.0-roughness*0.5)*(0.35+0.65*fresnel),0.0,0.85);
  vec3 sharp=fakeSkyReflection(R);
  vec3 blurred=mix(uSkyHorizon,uSkyZenith,0.5);
  vec3 env=mix(blurred,sharp,clamp(1.0-roughness,0.0,1.0));
  return mix(base,env*1.05,envAmt);
}
vec3 skyLightTerm(vec3 N,vec3 toSky,vec3 P){
  float amb=mix(0.18,0.08,uNight);        // fill basso = ombre profonde (quasi nere)
  float dir=mix(0.82,0.18,uNight);        // sole forte: le facce illuminate restano brillanti
  float vis=skyLightVisibility(P,N,toSky);
  // ombra portata scende fino a 0.12 = quasi nera ma non del tutto
  float lam=max(dot(N,toSky),0.0)*dir*mix(0.12,1.0,vis);
  return uAmbientTint*amb*contactAO(P)+uSunColor*lam;
}
)"

inline const char* kPartFS = R"(#version 330 core
in vec3 vN,vW; out vec4 FragColor;
in vec3 vLocalN;
in vec3 vLocalPos;
uniform vec3 uColor,uSun,uCamPos;
uniform float uEmissive,uSpecular,uRoughness,uAlpha;
uniform int uMaterial;
uniform sampler2D uMaterialTex;
uniform int uLightCount;
uniform vec3 uLightPos[8];
uniform vec3 uLightCol[8];
uniform float uLightRange[8];
)" BLOCKY_SHADOW_GLSL R"(
// Box-mapping: sceglie la faccia dall'asse dominante della normale locale,
// cosi' la texture non appare stirata sui lati come farebbe una singola
// proiezione planare.
vec2 boxUV(vec3 localPos, vec3 localN){
  vec3 an=abs(localN);
  if(an.x>=an.y && an.x>=an.z) return localPos.zy;
  if(an.y>=an.x && an.y>=an.z) return localPos.xz;
  return localPos.xy;
}
void main(){
  vec3 N=normalize(vN);
  vec3 Ls=normalize(-uSun);
  vec3 sun=skyLightTerm(N,Ls,vW);
  vec3 dyn=vec3(0.0);
  int n=min(uLightCount,8);
  for(int i=0;i<n;++i){
    vec3 toL=uLightPos[i]-vW;
    float d=length(toL);
    float r=max(uLightRange[i],0.05);
    float a=clamp(1.0-d/r,0.0,1.0);
    if(a<=0.002) continue;
    a*=a;
    vec3 Ld=toL/max(d,1e-4);
    float lam=max(dot(N,Ld),0.0);
    // ombra solo sulle facce rivolte alla luce (le altre sono gia' scure)
    float vis=(lam>0.0)?lightVisibility(vW,uLightPos[i]):1.0;
    dyn+=uLightCol[i]*a*(0.35+lam*0.90)*mix(0.25,1.0,vis);
  }
  // specular Blinn (Metal/Glass)
  vec3 V=normalize(uCamPos-vW);
  vec3 H=normalize(Ls+V);
  float shiny=mix(8.0,96.0,clamp(1.0-uRoughness,0.0,1.0));
  float spec=pow(max(dot(N,H),0.0),shiny)*uSpecular;
  // wood/grass striping leggero
  vec3 base=uColor;
  if(uMaterial==4){ // Wood — texture reale, colore della part solo come tinta leggera
    vec2 uv=boxUV(vLocalPos,vLocalN)/8.0; // 1 tile ogni 8 stud, non 2 — niente piu' effetto "voxel"
    vec3 tex=texture(uMaterialTex,uv).rgb;
    base=tex*mix(vec3(1.0),uColor,0.35);
  } else if(uMaterial==5){ // Concrete
    float nse=fract(sin(dot(vW.xz,vec2(12.9898,78.233)))*43758.5453);
    base*=mix(0.88,1.05,nse);
  } else if(uMaterial==6){ // Grass
    base*=mix(vec3(0.75,1.05,0.7),vec3(1.0),0.35+0.15*sin(vW.x*2.0));
  } else if(uMaterial==9){ // Brick
    float bx=floor(vW.x*2.0);
    float by=floor(vW.y*2.5);
    float cell=mod(bx+by,2.0);
    base*=mix(0.82,1.08,cell);
  }
  float em=clamp(uEmissive,0.0,2.0);
  if(uMaterial==10){ // ForceField pulse
    em+=0.25+0.25*sin(vW.x*4.0+vW.y*3.0+vW.z*2.0);
  }
  vec3 shade=base*(sun+dyn)+vec3(spec);
  vec3 glow=base*(mix(1.20,1.85,uNight)+em*mix(0.95,1.6,uNight));
  vec3 rgb=mix(shade,max(shade,glow),clamp(em,0.0,1.0));
  rgb=applyEnvReflection(rgb,N,V,uSpecular,uRoughness);
  rgb=applyFog(rgb,vW,uCamPos);
  FragColor=vec4(rgb,clamp(uAlpha,0.05,1.0));
})";

inline const char* kStudFS = R"(#version 330 core
in vec3 vN,vW;
in vec3 vLocalN;
in vec2 vStudUV;
out vec4 FragColor;
uniform vec3 uColor,uSun,uCamPos;
uniform sampler2D uStud;
uniform float uStudScale;
uniform float uEmissive,uSpecular,uRoughness,uAlpha;
uniform int uMaterial;
uniform int uLightCount;
uniform vec3 uLightPos[8];
uniform vec3 uLightCol[8];
uniform float uLightRange[8];
)" BLOCKY_SHADOW_GLSL R"(
void main(){
  vec3 N=normalize(vN);
  vec3 Ls=normalize(-uSun);
  vec3 sun=skyLightTerm(N,Ls,vW);
  vec3 dyn=vec3(0.0);
  int n=min(uLightCount,8);
  for(int i=0;i<n;++i){
    vec3 toL=uLightPos[i]-vW;
    float d=length(toL);
    float r=max(uLightRange[i],0.05);
    float a=clamp(1.0-d/r,0.0,1.0);
    if(a<=0.002) continue;
    a*=a;
    vec3 Ld=toL/max(d,1e-4);
    float lam=max(dot(N,Ld),0.0);
    float vis=(lam>0.0)?lightVisibility(vW,uLightPos[i]):1.0;
    dyn+=uLightCol[i]*a*(0.35+lam*0.90)*mix(0.25,1.0,vis);
  }
  vec3 V=normalize(uCamPos-vW);
  vec3 H=normalize(Ls+V);
  float shiny=mix(8.0,96.0,clamp(1.0-uRoughness,0.0,1.0));
  float spec=pow(max(dot(N,H),0.0),shiny)*uSpecular;
  vec3 base=uColor;
  // Faccia +Y in spazio oggetto → stud restano attaccati al Part se ruoti
  if(normalize(vLocalN).y>0.85){
    vec2 uv=vStudUV/max(uStudScale,0.5);
    vec3 stud=texture(uStud,uv).rgb;
    base=uColor*stud*1.12;
  }
  float em=clamp(uEmissive,0.0,2.0);
  vec3 shade=base*(sun+dyn)+vec3(spec);
  vec3 glow=base*(mix(1.20,1.85,uNight)+em*mix(0.95,1.6,uNight));
  vec3 rgb=mix(shade,max(shade,glow),clamp(em,0.0,1.0));
  rgb=applyEnvReflection(rgb,N,V,uSpecular,uRoughness);
  rgb=applyFog(rgb,vW,uCamPos);
  FragColor=vec4(rgb,clamp(uAlpha,0.05,1.0));
})";

inline const char* kGlesStudAtmosFS = R"(#version 320 es
precision mediump float;
in vec3 vNormal;
in vec3 vWorld;
in vec3 vLocalN;
in vec2 vStudUV;
out vec4 FragColor;
uniform vec3 uColor;
uniform vec3 uSun;
uniform vec3 uCamPos;
uniform sampler2D uStud;
uniform float uStudScale;
const vec3 SKY_ZENITH=vec3(0.53,0.72,0.92);
const vec3 SKY_HORIZON=vec3(0.69,0.84,0.95);
void main(){
  vec3 N=normalize(vNormal);
  vec3 L=normalize(-uSun);
  float diff=max(dot(N,L),0.0);
  float lit=0.40+diff*0.70;
  vec3 base=uColor;
  if(normalize(vLocalN).y>0.85){
    vec2 uv=vStudUV/max(uStudScale,0.5);
    vec3 stud=texture(uStud,uv).rgb;
    base=uColor*stud*1.10;
  }
  vec3 shaded=base*lit;
  float dist=length(vWorld-uCamPos);
  float fog=1.0-exp(-dist*0.0065);
  vec3 sky=mix(SKY_HORIZON,SKY_ZENITH,clamp(vWorld.y*0.02+0.5,0.0,1.0));
  FragColor=vec4(mix(shaded,sky,fog*0.55),1.0);
})";

inline const char* kPartVS = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 uModel,uView,uProj;
out vec3 vN,vW;
out vec3 vLocalN;
out vec2 vStudUV;
out vec3 vLocalPos;
void main(){
  vec4 w=uModel*vec4(aPos,1.);
  vW=w.xyz;
  vN=mat3(uModel)*aNormal;
  vLocalN=aNormal;
  // UV stud in metri locali (XZ del Part) — indipendenti dalla rotazione mondo
  vStudUV=vec2(aPos.x*length(uModel[0].xyz), aPos.z*length(uModel[2].xyz));
  // Posizione locale in metri reali (per il box-mapping delle texture materiale)
  vLocalPos=aPos*vec3(length(uModel[0].xyz), length(uModel[1].xyz), length(uModel[2].xyz));
  gl_Position=uProj*uView*w;
})";

inline const char* kMeshTexVS = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec3 aColor; // colore per-vertice (bianco se il file non ne ha)
uniform mat4 uModel,uView,uProj;
out vec3 vN,vW;
out vec2 vUV;
out vec3 vCol;
void main(){
  vec4 w=uModel*vec4(aPos,1.);
  vW=w.xyz;
  vN=mat3(uModel)*aNormal;
  vUV=aUV;
  vCol=aColor;
  gl_Position=uProj*uView*w;
})";

inline const char* kMeshTexFS = R"(#version 330 core
in vec3 vN,vW;
in vec2 vUV;
in vec3 vCol;
out vec4 FragColor;
uniform vec3 uColor,uSun;
uniform vec3 uCamPos;
uniform sampler2D uTex;
uniform int uUseTex;
uniform float uAlpha;
// Materiale importato dal file (glTF/FBX/OBJ)
uniform vec3 uEmissiveCol;
uniform sampler2D uEmissiveTex;
uniform int uUseEmissiveTex;
uniform float uSpecular;
uniform float uRoughness;
uniform int uLightCount;
uniform vec3 uLightPos[8];
uniform vec3 uLightCol[8];
uniform float uLightRange[8];
)" BLOCKY_SHADOW_GLSL R"(
void main(){
  vec3 N=normalize(vN), L=normalize(-uSun);
  vec3 lit=skyLightTerm(N,L,vW)*1.18;
  vec3 dyn=vec3(0.0);
  int n=min(uLightCount,8);
  for(int i=0;i<n;++i){
    vec3 toL=uLightPos[i]-vW;
    float d=length(toL);
    float r=max(uLightRange[i],0.05);
    float att=clamp(1.0-d/r,0.0,1.0);
    if(att<=0.002) continue;
    att*=att;
    float lam=max(dot(N,toL/max(d,1e-4)),0.0);
    float vis=(lam>0.0)?lightVisibility(vW,uLightPos[i]):1.0;
    dyn+=uLightCol[i]*att*(0.35+lam*0.90)*mix(0.25,1.0,vis);
  }
  vec3 base=uColor*vCol;
  float a=uAlpha;
  if(uUseTex!=0){
    vec4 tex=texture(uTex,vUV);
    if(tex.a<0.08) discard; // cutout (fogliame, reti, decal)
    base*=tex.rgb;
    a*=tex.a;
  }
  // Riflesso speculare dal metallic/roughness del materiale
  vec3 V=normalize(uCamPos-vW);
  vec3 Hv=normalize(L+V);
  float shiny=mix(8.0,96.0,clamp(1.0-uRoughness,0.0,1.0));
  float spec=pow(max(dot(N,Hv),0.0),shiny)*uSpecular*(1.0-uNight*0.7);
  // Emissive: additiva come in glTF (non moltiplica il base color)
  vec3 emis=uEmissiveCol;
  if(uUseEmissiveTex!=0){
    vec3 et=texture(uEmissiveTex,vUV).rgb;
    emis=(uEmissiveCol==vec3(0.0))?et:uEmissiveCol*et;
  }
  vec3 rgb=base*(lit+dyn)+vec3(spec)+emis*mix(1.5,2.2,uNight);
  rgb=applyEnvReflection(rgb,N,V,uSpecular,uRoughness);
  rgb=applyFog(rgb,vW,uCamPos);
  FragColor=vec4(rgb,a);
})";

inline const char* kGlesPartVS = R"(#version 320 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 uModel,uView,uProj;
out vec3 vNormal;
out vec3 vWorld;
void main(){
  vec4 w=uModel*vec4(aPos,1.0);
  vWorld=w.xyz;
  vNormal=mat3(uModel)*aNormal;
  gl_Position=uProj*uView*w;
})";
