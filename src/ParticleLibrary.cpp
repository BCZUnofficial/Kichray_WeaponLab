#include "ParticleLibrary.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include <set>
#include <stdexcept>

namespace lab { namespace {
struct Reader {
    const uint8_t* p=nullptr;size_t n=0,at=0;
    void need(size_t k)const{if(k>n-at)throw std::runtime_error("Обрезанный блок particles.xr.");}
    uint32_t u(){need(4);uint32_t v=uint32_t(p[at])|(uint32_t(p[at+1])<<8)|(uint32_t(p[at+2])<<16)|(uint32_t(p[at+3])<<24);at+=4;return v;}
    uint16_t u16(){need(2);auto v=uint16_t(p[at]|(p[at+1]<<8));at+=2;return v;}
    float f(){uint32_t u32=u();float v;std::memcpy(&v,&u32,4);if(!std::isfinite(v))throw std::runtime_error("NaN/Inf в particles.xr.");return v;}
    V3 v(){return {f(),f(),f()};}
    std::string s(){size_t start=at;while(at<n&&p[at]&&at-start<4096)++at;if(at==n||at-start==4096)throw std::runtime_error("Неверная строка particles.xr.");std::string r(reinterpret_cast<const char*>(p+start),at-start);++at;return r;}
    Reader take(size_t size){need(size);Reader r{p+at,size};at+=size;return r;}
};
std::map<uint32_t,Reader> chunks(Reader r){std::map<uint32_t,Reader> result;while(r.at<r.n){uint32_t id=r.u(),size=r.u();if(id&0x80000000u)throw std::runtime_error("Сжатые LZH-блоки particles.xr не поддерживаются; сохраните библиотеку без сжатия в Particle Editor.");if(result.size()>100000||!result.emplace(id,r.take(size)).second)throw std::runtime_error("Дубликат/слишком много блоков particles.xr.");}return result;}
Reader required(const std::map<uint32_t,Reader>& c,uint32_t id){auto it=c.find(id);if(it==c.end())throw std::runtime_error("В particles.xr отсутствует обязательный блок "+std::to_string(id));return it->second;}
V3 add(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}V3 sub(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}V3 scale(V3 a,float b){return {a.x*b,a.y*b,a.z*b};}
float length(V3 a){return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);}V3 norm(V3 a){float l=length(a);return l>1e-12f?scale(a,1/l):V3{};}
void warning(std::vector<std::string>& w,const std::string& s){if(std::find(w.begin(),w.end(),s)==w.end())w.push_back(s);}
float random(std::mt19937& r){return float(r()>>8)*(1.f/16777216.f);}
float gaussian(std::mt19937& r,float sigma){if(sigma==0)return 0;float y;do{y=-std::log(std::max(1e-7f,random(r)));}while(random(r)>std::exp(-(y-1)*(y-1)/2));return y*sigma*(random(r)>.5f?1.f:-1.f)/.7975f;}
struct Domain {uint32_t type;V3 p1,p2,u,v;float r1,r2,r1sq,r2sq;};
Domain domain(Reader& r){Domain d{r.u(),r.v(),r.v(),r.v(),r.v(),r.f(),r.f(),r.f(),r.f()};if(d.type>10)throw std::runtime_error("Неизвестный pDomain.");return d;}
V3 generate(const Domain& d,std::mt19937& rng){auto R=[&](){return random(rng);};float a,b;
    switch(d.type){case 0:case 3:return d.p1;case 1:return add(d.p1,scale(d.p2,R()));
    case 4:return {d.p1.x+(d.p2.x-d.p1.x)*R(),d.p1.y+(d.p2.y-d.p1.y)*R(),d.p1.z+(d.p2.z-d.p1.z)*R()};
    case 2:a=R();b=R();if(a+b>=1){a=1-a;b=1-b;}return add(d.p1,add(scale(d.u,a),scale(d.v,b)));
    case 10:a=R();b=R();return add(d.p1,add(scale(d.u,a),scale(d.v,b)));
    case 5:{auto p=norm({R()-.5f,R()-.5f,R()-.5f});return add(d.p1,scale(p,d.r1==d.r2?d.r1:d.r2+R()*(d.r1-d.r2)));}
    case 6:case 7:case 9:{float t=d.type==9?0:R(),angle=R()*6.28318530718f,rad=d.r2+R()*(d.r1-d.r2);if(d.type==7)rad*=t;return add(d.p1,add(scale(d.p2,t),add(scale(d.u,rad*std::cos(angle)),scale(d.v,rad*std::sin(angle)))));}
    case 8:return {d.p1.x+gaussian(rng,d.r1),d.p1.y+gaussian(rng,d.r1),d.p1.z+gaussian(rng,d.r1)};default:return {};}
}
bool inside(const Domain& d,V3 p){auto q=sub(p,d.p1);switch(d.type){case 0:return length(q)<1e-5f;case 3:return q.x*d.p2.x+q.y*d.p2.y+q.z*d.p2.z>=0;case 4:return p.x>=d.p1.x&&p.x<=d.p2.x&&p.y>=d.p1.y&&p.y<=d.p2.y&&p.z>=d.p1.z&&p.z<=d.p2.z;case 5:{float r=length(q);return r>=d.r2&&r<=d.r1;}default:return false;}}
bool supported(uint32_t t){return t==3||t==4||t==8||t==10||t==12||(t>=15&&t<=17)||(t>=19&&t<=28);}
}
bool ParticleLibrary::load(const std::vector<uint8_t>& bytes,std::string& error){
    error.clear();try{
        if(bytes.empty()||bytes.size()>256u*1024u*1024u)throw std::runtime_error("Нет particles.xr или размер превышает 256 МиБ.");
        ParticleLibrary next;auto root=chunks({bytes.data(),bytes.size()});if(required(root,1).u16()!=1)throw std::runtime_error("Неподдерживаемая версия particles.xr (нужна 1).");
        const int sizes[]={80,80,-1,4,20,32,12,12,12,92,8,12,0,36,24,68,68,68,4,72,72,372,8,20,24,16,16,16,16,36,28,24};
        if(root.count(3))for(const auto& item:chunks(root.at(3))){auto c=chunks(item.second);if(required(c,1).u16()!=1)throw std::runtime_error("Неподдерживаемая версия PED.");
            Effect e;e.layer.name=required(c,2).s();e.maximum=required(c,3).u();e.layer.flags=required(c,5).u();if(e.layer.name.empty()||e.maximum>1000000)throw std::runtime_error("Неверное имя/лимит PED.");
            if(e.layer.flags&1){auto r=required(c,7);e.layer.shader=r.s();e.layer.texture=r.s();}
            if(e.layer.flags&(1<<10)){auto r=required(c,6);e.layer.frameU=r.f();e.layer.frameV=r.f();r.f();r.f();e.layer.frameColumns=r.u();e.layer.frameCount=r.u();e.layer.frameSpeed=r.f();
                if(e.layer.frameU<=0||e.layer.frameU>1||e.layer.frameV<=0||e.layer.frameV>1||!e.layer.frameColumns||e.layer.frameColumns>65536||!e.layer.frameCount||e.layer.frameCount>65536)throw std::runtime_error("Неверная сетка кадров PED.");}
            if(e.layer.flags&(1<<14))e.limit=required(c,8).f();if(c.count(0x22))e.layer.velocityScale=c.at(0x22).v();if(c.count(0x25))e.layer.defaultRotation=c.at(0x25).v();else e.layer.defaultRotation={-1.5707963f,0,0};
            auto actions=required(c,4);auto count=actions.u();if(count>4096)throw std::runtime_error("Слишком много действий PED.");
            for(uint32_t i=0;i<count;++i){Action a;a.type=actions.u();a.flags=actions.u();const auto savedType=actions.u();if(a.type>=32||sizes[a.type]<0||savedType!=a.type)throw std::runtime_error("Неизвестное/несогласованное действие PAPI.");auto r=actions.take(size_t(sizes[a.type]));a.data.assign(r.p,r.p+r.n);e.actions.push_back(std::move(a));}
            if(actions.at!=actions.n)throw std::runtime_error("Лишние байты списка действий PED.");auto name=e.layer.name;if(!next.effects_.emplace(name,std::move(e)).second)throw std::runtime_error("Дубликат имени PED.");next.entries_.push_back({name,false});}
        if(root.count(4))for(const auto& item:chunks(root.at(4))){auto c=chunks(item.second);if(required(c,1).u16()!=3)throw std::runtime_error("Неподдерживаемая версия PGD.");const auto name=required(c,2).s();Group g;auto r=required(c,c.count(4)?4:7);uint32_t count=r.u();if(count>4096)throw std::runtime_error("Слишком много элементов PGD.");
            for(uint32_t i=0;i<count;++i){Member m;m.effect=r.s();m.onPlay=r.s();if(c.count(4)){m.onBirth=r.s();m.onDeath=r.s();}m.start=r.f();m.end=r.f();m.flags=r.u();g.members.push_back(std::move(m));}
            if(c.count(5))g.limit=c.at(5).f();if(name.empty()||next.effects_.count(name)||!next.groups_.emplace(name,std::move(g)).second)throw std::runtime_error("Дубликат/пустое имя PGD.");next.entries_.push_back({name,true});}
        std::sort(next.entries_.begin(),next.entries_.end(),[](const auto& a,const auto& b){return a.name<b.name;});*this=std::move(next);return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool ParticleLibrary::contains(const std::string& n)const{return effects_.count(n)||groups_.count(n);}
ParticleLayer ParticleLibrary::sampleEffect(const Effect& e,float time,uint32_t seed,std::vector<std::string>& warnings,float stop)const{
    auto layer=e.layer;std::mt19937 rng(seed);const size_t cap=std::min(e.maximum,4096u);constexpr float dt=.033f;
    if(e.maximum>cap)warning(warnings,"Предпросмотр ограничен 4096 частицами на эффект.");
    if(layer.flags&(1<<16))warning(warnings,"Столкновения с уровнем в статичном предпросмотре отсутствуют.");
    if(layer.flags&(1<<13))warning(warnings,"RandomPlayback: предпросмотр использует прямое направление кадров.");
    for(const auto& a:e.actions)if(!supported(a.type))warning(warnings,"Упрощённый предпросмотр: действие PAPI "+std::to_string(a.type)+" не исполняется.");
    auto& p=layer.particles;
    for(int step=0;step<int(std::clamp(time,0.f,4.f)/dt);++step){
        for(const auto& action:e.actions){Reader r{action.data.data(),action.data.size()};const auto type=action.type;
            if(type==21){Domain pos=domain(r),vel=domain(r),rot=domain(r),size=domain(r),color=domain(r);float alpha=r.f(),rate=r.f(),age=r.f(),sigma=r.f();r.v();r.f();
                if((action.flags&(1u<<30))||((layer.flags&(1<<14))&&(step+1)*dt>e.limit)||(stop>=0&&(step+1)*dt>stop))continue;
                double expected=std::clamp(double(rate)*dt,0.0,1000000.0);size_t count=size_t(expected)+(random(rng)<expected-std::floor(expected));count=std::min(count,cap-p.size());
                for(size_t j=0;j<count;++j){ParticleSample s;s.position=generate(pos,rng);s.size=generate(size,rng);if(action.flags&(1u<<29))s.size={s.size.x,s.size.x,s.size.x};s.rotation=generate(rot,rng);s.velocity=generate(vel,rng);auto c=generate(color,rng);s.r=c.x;s.g=c.y;s.b=c.z;s.a=alpha;s.age=age+gaussian(rng,sigma);if(layer.flags&(1<<12))s.frame=float(rng()%layer.frameCount);p.push_back(s);}continue;
            }
            if(type==10){float age=r.f();bool less=r.u()!=0;p.erase(std::remove_if(p.begin(),p.end(),[&](const auto& s){return (s.age>=age)!=less;}),p.end());continue;}
            if(type==19||type==20){bool kill=r.u()!=0;auto d=domain(r);if(d.type!=0&&d.type!=3&&d.type!=4&&d.type!=5){warning(warnings,"Упрощённый предпросмотр: Sink для этого pDomain не исполняется.");continue;}p.erase(std::remove_if(p.begin(),p.end(),[&](const auto& s){return inside(d,type==19?s.position:s.velocity)==kill;}),p.end());continue;}
            if(type==3)continue;
            if(type==12){for(auto& s:p){s.age+=dt;s.position=add(s.position,scale(s.velocity,dt));}continue;}
            if(type==8){auto d=r.v();for(auto& s:p)s.velocity=add(s.velocity,scale(d,dt));continue;}
            if(type==4){auto d=r.v();float lo=r.f(),hi=r.f();for(auto& s:p){float speed=length(s.velocity);if(speed*speed>=lo&&speed*speed<=hi){s.velocity.x*=1-(1-d.x)*dt;s.velocity.y*=1-(1-d.y)*dt;s.velocity.z*=1-(1-d.z)*dt;}}continue;}
            if(type>=15&&type<=17){auto d=domain(r);for(auto& s:p){auto v=generate(d,rng);if(type==15)s.velocity=add(s.velocity,scale(v,dt));else if(type==16)s.position=add(s.position,scale(v,dt));else s.velocity=v;}continue;}
            if(type==22){float lo=r.f(),hi=r.f();for(auto& s:p){float l=length(s.velocity);if(l>0)s.velocity=scale(s.velocity,std::clamp(l,std::max(0.f,lo),std::max(std::max(0.f,lo),hi))/l);}continue;}
            if(type==23){auto c=r.v();float a=r.f(),factor=r.f()*dt;for(auto& s:p){s.r+= (c.x-s.r)*factor;s.g+=(c.y-s.g)*factor;s.b+=(c.z-s.b)*factor;s.a+=(a-s.a)*factor;}continue;}
            if(type==24){auto target=r.v(),k=r.v();for(auto& s:p){s.size.x+=(target.x-s.size.x)*k.x*dt;s.size.y+=(target.y-s.size.y)*k.y*dt;s.size.z+=(target.z-s.size.z)*k.z*dt;}continue;}
            if(type==25||type==26){auto target=r.v();float k=r.f()*dt;for(auto& s:p){float sign=s.rotation.x>=0?k:-k;s.rotation.x+=(std::abs(target.x)-std::abs(s.rotation.x))*sign;s.rotation.y+=(std::abs(target.y)-std::abs(s.rotation.y))*sign;s.rotation.z+=(std::abs(target.z)-std::abs(s.rotation.z))*sign;}continue;}
            if(type==27||type==28){auto v=r.v();float k=r.f()*dt;for(auto& s:p)s.velocity=add(s.velocity,scale(sub(v,s.velocity),k));}
        }
        if(layer.flags&(1<<11))for(auto& s:p){s.frame=std::fmod(s.frame+layer.frameSpeed*dt,float(layer.frameCount));if(s.frame<0)s.frame+=layer.frameCount;}
    }
    p.erase(std::remove_if(p.begin(),p.end(),[](const auto& s){return !std::isfinite(s.position.x+s.position.y+s.position.z+s.size.x+s.size.y+s.size.z+s.rotation.x+s.rotation.y+s.rotation.z+s.velocity.x+s.velocity.y+s.velocity.z+s.frame+s.r+s.g+s.b+s.a)||length(s.position)>1e5f||length(s.size)>1e4f;}),p.end());
    return layer;
}
ParticleSnapshot ParticleLibrary::snapshot(const std::string& name,float time,uint32_t seed)const{
    ParticleSnapshot out;if(!std::isfinite(time)||time<0||time>4){out.warnings.push_back("Время кадра должно быть от 0 до 4 секунд.");return out;}
    try{auto addEffect=[&](const std::string& n,float t,float stop){auto e=effects_.find(n);if(e==effects_.end()){warning(out.warnings,"Не найден PED: "+n);return;}if(out.layers.size()>=64){warning(out.warnings,"Предпросмотр ограничен 64 слоями.");return;}out.layers.push_back(sampleEffect(e->second,t,seed++,out.warnings,stop));};
        auto e=effects_.find(name);if(e!=effects_.end()){addEffect(name,time,-1);return out;}auto g=groups_.find(name);if(g==groups_.end()){out.warnings.push_back("Эффект не найден: "+name);return out;}
        for(const auto& m:g->second.members){if(!(m.flags&4)||time<m.start)continue;if(time>=m.end&&!(m.flags&1))continue;float stop=std::max(0.f,m.end-m.start);if(g->second.limit>0)stop=std::min(stop,std::max(0.f,g->second.limit-m.start));addEffect(m.effect,time-m.start,stop);
            if((m.flags&2)&&!m.onPlay.empty())warning(out.warnings,"Упрощённый предпросмотр: дочерний эффект OnPlay следует за частицами только в игре.");
            if(m.flags&((1<<5)|(1<<6)))warning(out.warnings,"Дочерние эффекты OnBirth/OnDeath не воспроизводятся в статичном предпросмотре.");}
    }catch(const std::exception& e){out.layers.clear();out.warnings.push_back(e.what());}return out;
}
bool ParticleShaderAdditive(const std::string& shader){return shader.find("add")!=std::string::npos;}
Image ParticleThumbnail(const ParticleSnapshot& snapshot,const std::function<Image(const std::string&)>& load,uint32_t size){
    size=std::clamp(size,64u,512u);Image out;out.w=out.h=size;out.pixels.assign(size_t(size)*size,{24,27,32,255});float extent=.02f;
    for(const auto& l:snapshot.layers)for(const auto& p:l.particles)extent=std::max(extent,std::max(std::abs(p.position.x),std::abs(p.position.y))+std::max(std::abs(p.size.x),std::abs(p.size.y)));
    float scalePx=size*.45f/extent;
    for(const auto& l:snapshot.layers){auto tex=load(l.texture);if(!tex.valid())continue;bool addBlend=ParticleShaderAdditive(l.shader);
        for(const auto& p:l.particles){float cx=size*.5f+p.position.x*scalePx,cy=size*.5f-p.position.y*scalePx,rx=std::max(.5f,std::abs(p.size.x)*scalePx*.5f),ry=std::max(.5f,std::abs(p.size.y)*scalePx*.5f);float angle=p.rotation.x,co=std::cos(angle),si=std::sin(angle),radius=std::hypot(rx,ry);uint32_t frame=uint32_t(std::max(0.f,p.frame))%l.frameCount;
            for(int y=std::max(0,int(cy-radius));y<std::min(int(size),int(cy+radius+1));++y)for(int x=std::max(0,int(cx-radius));x<std::min(int(size),int(cx+radius+1));++x){float dx=x+.5f-cx,dy=y+.5f-cy,u=(dx*co+dy*si)/rx*.5f+.5f,v=(-dx*si+dy*co)/ry*.5f+.5f;if(u<0||u>=1||v<0||v>=1)continue;u=(frame%l.frameColumns+u)*l.frameU;v=(frame/l.frameColumns+v)*l.frameV;const auto& t=tex.pixels[size_t(std::min(tex.h-1,uint32_t(std::max(0.f,v)*tex.h)))*tex.w+std::min(tex.w-1,uint32_t(std::max(0.f,u)*tex.w))];float a=std::clamp(t.a/255.f*p.a,0.f,1.f);auto& dst=out.pixels[size_t(y)*size+x];auto blend=[&](uint8_t old,uint8_t val,float color){return uint8_t(std::clamp(old*(addBlend?1.f:1-a)+val*std::clamp(color,0.f,1.f)*a,0.f,255.f));};dst.r=blend(dst.r,t.r,p.r);dst.g=blend(dst.g,t.g,p.g);dst.b=blend(dst.b,t.b,p.b);}
        }
    }return out;
}
}
