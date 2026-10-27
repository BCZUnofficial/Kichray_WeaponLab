#include "WeaponSimulation.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <locale>
#include <random>
#include <set>
#include <sstream>
#include <utility>
namespace lab {
namespace {
constexpr double Pi=3.14159265358979323846;
std::string trim(std::string s){const auto a=s.find_first_not_of(" \t\r\n\"");if(a==std::string::npos)return {};const auto b=s.find_last_not_of(" \t\r\n\"");return s.substr(a,b-a+1);}
std::string get(const SimulationConfig& c,const std::string& key){const auto it=c.find(key);return it==c.end()?std::string{}:trim(it->second);}
std::string weaponClass(const SimulationConfig& c){auto s=get(c,"class");for(auto& ch:s)if(ch>='a'&&ch<='z')ch=char(ch-'a'+'A');return s;}
bool numeric(const std::string& text,double& value){std::istringstream in(trim(text));in.imbue(std::locale::classic());if(!(in>>value)||!std::isfinite(value))return false;in>>std::ws;return in.eof();}
bool number(const SimulationConfig& c,const char* key,double& value,std::string& error){if(!numeric(get(c,key),value)){error=std::string("Нет корректного числового параметра ")+key;return false;}return true;}
bool optional(const SimulationConfig& c,const char* key,double fallback,double& value,std::string& error){value=fallback;return c.count(key)?number(c,key,value,error):true;}
bool singleShotClass(const std::string& c){static const std::set<std::string> classes={"WP_SVD","WP_SVU","WP_HPSA","WP_PM","WP_USP45","WP_WALTH","WP_SHOTG","WP_BM16","W_HPSA","W_PM","W_FORT","W_SHOTGN","W_SVD","W_SVU","W_WALTHR","W_USP45","W_BM16"};return classes.count(c)!=0;}
void warn(SimulationResult& r,const std::string& text){if(std::find(r.warnings.begin(),r.warnings.end(),text)==r.warnings.end())r.warnings.push_back(text);}
struct RNG {
    std::mt19937 generator;
    explicit RNG(uint32_t seed):generator(seed){}
    double unit(){return (double(generator())+.5)/4294967296.0;}
    double gaussian(double sigma){if(sigma==0)return 0;double y=0;for(int i=0;i<1024;++i){y=-std::log(unit());if(unit()<=std::exp(-(y-1)*(y-1)*.5))break;}return (generator()&1?1:-1)*y*sigma/0.7975;}
};
void lifetime(SimulationResult& r,const std::vector<double>& wear,double condition){
    long double cycle=0;for(double d:wear)cycle+=d;
    if(condition==0){r.lifetimeFinite=true;return;}
    if(cycle<=0){r.lifetimeFinite=false;return;}
    r.lifetimeFinite=true;
    // Repeated full magazines have an identical shot-counter pattern. Skip whole cycles.
    long double complete=std::floor(static_cast<long double>(condition)/cycle);
    if(complete>0&&complete*cycle>=static_cast<long double>(condition))complete-=1;
    const long double maxCycles=static_cast<long double>(std::numeric_limits<uint64_t>::max())/wear.size();
    if(complete>maxCycles){r.lifetimeCapped=true;r.shotsToZero=std::numeric_limits<uint64_t>::max();return;}
    uint64_t shots=uint64_t(complete)*uint64_t(wear.size());
    long double remaining=static_cast<long double>(condition)-complete*cycle;
    for(double d:wear){if(shots==std::numeric_limits<uint64_t>::max()){r.lifetimeCapped=true;break;}++shots;remaining-=d;if(remaining<=cycle*1e-12L)break;}
    r.shotsToZero=shots;r.fullMagazinesToZero=shots/uint64_t(wear.size());r.remainingRoundsToZero=int(shots%uint64_t(wear.size()));
}
void spread(const SimulationRequest& q,SimulationResult& r){
    std::string error;double base=0,conditionFactor=0,dec=0,rpm=0,mag=0,kdisp=0,impair=0,pellets=0,misfire=0,misfireK=1,preferred=-1;
    if(!number(q.weapon,"fire_dispersion_base",base,error)||!number(q.weapon,"fire_dispersion_condition_factor",conditionFactor,error)||!number(q.weapon,"condition_shot_dec",dec,error)||!number(q.weapon,"rpm",rpm,error)||!number(q.weapon,"ammo_mag_size",mag,error)||!number(q.ammo,"k_disp",kdisp,error)||!number(q.ammo,"impair",impair,error)||!number(q.ammo,"buck_shot",pellets,error)||!number(q.weapon,"misfire_probability",misfire,error)||!optional(q.weapon,"misfire_condition_k",1,misfireK,error)||!optional(q.weapon,"preffered_fire_mode",-1,preferred,error)){
        warn(r,"Разброс/износ недоступны: "+error);return;
    }
    if(base<0||conditionFactor<0||dec<0||rpm<=0||rpm>1e9||mag<1||mag>10000||std::floor(mag)!=mag||kdisp<0||impair<0||pellets<1||pellets>1000||std::floor(pellets)!=pellets||misfire<0||misfireK<0||preferred<-1||preferred>32767||std::floor(preferred)!=preferred){warn(r,"Разброс/износ: значения вне допустимых границ; магазин 1–10000, дробин 1–1000, неотрицательные коэффициенты и RPM > 0.");return;}
    if(!std::isfinite(q.condition)||q.condition<0||q.condition>1||!std::isfinite(q.triggerPause)||q.triggerPause<0||q.triggerPause>60||!std::isfinite(q.spreadDistance)||q.spreadDistance<10||q.spreadDistance>100){warn(r,"Некорректные настройки стенда: состояние 0–1, дистанция разброса 10–100 м, пауза 0–60 с.");return;}
    if(std::find(r.fireModes.begin(),r.fireModes.end(),q.fireMode)==r.fireModes.end()){warn(r,"Выбранный режим стрельбы недоступен этому классу оружия.");return;}
    r.magazineSize=int(mag);r.pelletsPerShot=int(pellets);r.rpm=rpm;r.shotInterval=60/rpm;r.initialCondition=q.condition;
    const int burst=q.fireMode==-1?r.magazineSize:q.fireMode;
    const bool increasing=q.weapon.count("fire_modes")&&preferred!=-1&&uint32_t(q.fireMode)>uint32_t(int(preferred));
    if(increasing)warn(r,"preffered_fire_mode увеличивает износ по номеру выстрела в очереди; AUTO сравнивается как UINT_MAX, как в master.");
    RNG rng(q.seed);double condition=q.condition,time=0;std::vector<double> wear;wear.reserve(r.magazineSize);
    constexpr size_t MaxPoints=50000;
    for(int i=0;i<r.magazineSize;++i){
        const int shotInTrigger=i%burst+1;
        if(i){time+=r.shotInterval;if(shotInTrigger==1)time+=q.triggerPause;}
        const double loss=dec*impair*(increasing?shotInTrigger:1);
        if(!std::isfinite(loss)){warn(r,"Переполнение при расчёте износа.");r.shots.clear();r.spread.clear();return;}
        wear.push_back(loss);const double before=condition;condition=std::clamp(condition-loss,0.0,1.0);
        // FireTrace changes condition before GetFireDispersion, once per cartridge, not once per pellet.
        const double dispersion=base*(Pi/180)*kdisp*(1+conditionFactor*(1-condition));
        if(!std::isfinite(dispersion)||dispersion>=Pi*.5){warn(r,"Разброс достиг 90°: проекция на плоскую мишень не определена; уменьшите параметры.");r.shots.clear();r.spread.clear();return;}
        const double mis=condition>.95?0:std::clamp(misfire+std::pow(1-condition,3)*misfireK,0.0,.99);
        r.shots.push_back({time,before,condition,dispersion,mis,shotInTrigger});
        if(i==0)r.firstDispersionRadians=dispersion;
        r.lastDispersionRadians=dispersion;
        r.maxConeRadius=std::max(r.maxConeRadius,q.spreadDistance*std::tan(dispersion));
        for(int pellet=0;pellet<r.pelletsPerShot&&r.spread.size()<MaxPoints;++pellet){
            const double alpha=std::clamp(rng.gaussian(dispersion/3),-dispersion,dispersion),theta=rng.unit()*Pi;
            const double radius=q.spreadDistance*std::tan(alpha);
            r.spread.push_back({radius*std::sin(theta),radius*std::cos(theta),time,condition,i,pellet});
        }
    }
    if(uint64_t(r.magazineSize)*uint64_t(r.pelletsPerShot)>MaxPoints)warn(r,"На мишени показаны только первые 50000 снарядов; износ рассчитан для полного магазина.");
    r.roundsFired=r.magazineSize;r.magazineDuration=time;r.conditionAfterMagazine=condition;r.magazineWear=q.condition-condition;
    lifetime(r,wear,q.condition);r.spreadValid=true;
}
void trajectory(const SimulationRequest& q,SimulationResult& r){
    double speed=0,maxDist=0,kdist=0,gravity=0,drag=0,step=0,minSpeed=0;std::string error;
    if(!number(q.weapon,"bullet_speed",speed,error)||!number(q.weapon,"fire_distance",maxDist,error)||!number(q.ammo,"k_dist",kdist,error)||!number(q.bulletManager,"gravity_const",gravity,error)||!number(q.bulletManager,"air_resistance_k",drag,error)||!number(q.bulletManager,"time_step",step,error)||!number(q.bulletManager,"min_bullet_speed",minSpeed,error)){warn(r,"Траектория недоступна: "+error+". Нужна секция [bullet_manager].");return;}
    maxDist*=kdist;
    if(!std::isfinite(maxDist)||speed<=0||speed>1e7||maxDist<=0||gravity<0||drag<0||step<1||step>1000||std::floor(step)!=step||minSpeed<0||drag*step/1000>=1||!std::isfinite(q.trajectoryDistance)||q.trajectoryDistance<100||q.trajectoryDistance>300){warn(r,"Траектория: некорректные параметры скорости/дальности/шага/сопротивления или дистанция вне 100–300 м.");return;}
    r.startSpeed=speed;r.maximumTravel=maxDist;r.trajectoryValid=true;
    const double dt=step/1000;LaboratoryBullet bullet;bullet.vx=speed;
    r.trajectory.push_back({0,0,0,speed});
    if(q.ammo.count("k_air_resistance"))warn(r,"В одиночной игре k_air_resistance патрона не участвует: master использует air_resistance_k из [bullet_manager].");
    for(size_t i=0;i<200000;++i){
        const auto previous=bullet;const double magnitude=std::hypot(bullet.vx,bullet.vy);
        const double range=std::min(magnitude*dt,std::max(0.0,maxDist-bullet.travelled));
        if(range<=0||magnitude<=0){r.stopReason="Достигнута fire_distance × k_dist.";break;}
        const double actualDt=range/magnitude;
        bullet.x+=bullet.vx*actualDt;bullet.y+=bullet.vy*actualDt;bullet.travelled+=range;bullet.time+=actualDt;
        if(bullet.x>=q.trajectoryDistance){
            const double f=(q.trajectoryDistance-previous.x)/(bullet.x-previous.x);
            const double y=previous.y+(bullet.y-previous.y)*f,t=previous.time+(bullet.time-previous.time)*f;
            r.trajectory.push_back({q.trajectoryDistance,y,t,magnitude});r.dropAtTarget=-y;r.timeAtTarget=t;r.speedAtTarget=magnitude;r.trajectoryReached=true;r.stopReason="Достигнута заданная дистанция.";break;
        }
        // X-Ray stops at maximum path length before the velocity update.
        if(bullet.travelled>=maxDist){r.trajectory.push_back({bullet.x,bullet.y,bullet.time,magnitude});r.stopReason="Пуля удалена по fire_distance × k_dist до заданной дистанции.";break;}
        bullet.vx*=1-drag*dt;bullet.vy=bullet.vy*(1-drag*dt)-gravity*dt;
        const double nextSpeed=std::hypot(bullet.vx,bullet.vy);
        r.trajectory.push_back({bullet.x,bullet.y,bullet.time,nextSpeed});
        if(!std::isfinite(nextSpeed)||!std::isfinite(bullet.y)){r.trajectoryValid=false;r.stopReason="Числовое переполнение траектории.";break;}
        if(nextSpeed<minSpeed){r.stopReason="Пуля удалена по min_bullet_speed до заданной дистанции.";break;}
        if(bullet.time>=120||bullet.x-previous.x<1e-10){r.stopReason="Остановлено на лимите стенда: 120 с / отсутствие продвижения.";break;}
    }
    if(!r.trajectoryReached&&!r.stopReason.empty())warn(r,r.stopReason);
}
}
bool IsSimulationFirearm(const SimulationConfig& c){static const std::set<std::string> classes={"WP_AK74","WP_LR300","WP_GROZA","WP_SVD","WP_HPSA","WP_PM","WP_SHOTG","WP_SVU","WP_USP45","WP_VAL","WP_VINT","WP_WALTH","WP_BM16","W_FN2000","W_AK74","W_LR300","W_HPSA","W_PM","W_FORT","W_SHOTGN","W_WMAGAZ","W_WMAGGL","W_SVD","W_SVU","W_VAL","W_VINT","W_WALTHR","W_USP45","W_GROZA","W_BM16"};return classes.count(weaponClass(c))!=0;}
std::vector<int> SimulationFireModes(const SimulationConfig& c){
    if(!IsSimulationFirearm(c))return {};
    if(singleShotClass(weaponClass(c)))return {1};
    if(!c.count("fire_modes"))return {-1};
    std::vector<int> modes;std::istringstream in(get(c,"fire_modes"));std::string item;
    while(std::getline(in,item,',')){double n=0;if(!numeric(item,n)||std::floor(n)!=n||(n!=-1&&(n<1||n>32767)))return {};if(std::find(modes.begin(),modes.end(),int(n))==modes.end())modes.push_back(int(n));}
    return modes;
}
void StepLaboratoryBullet(LaboratoryBullet& b,double dt,double drag,double gravity){const double speed=std::hypot(b.vx,b.vy);b.x+=b.vx*dt;b.y+=b.vy*dt;b.travelled+=speed*dt;b.time+=dt;b.vx*=1-drag*dt;b.vy=b.vy*(1-drag*dt)-gravity*dt;}
SimulationResult SimulateWeapon(const SimulationRequest& q){
    SimulationResult r;r.supported=IsSimulationFirearm(q.weapon);r.weaponType=weaponClass(q.weapon);r.ammoName=q.ammoSection;
    if(!r.supported){r.error="Стенд доступен только для поддерживаемого пулевого оружия; боеприпасы, гранаты, ножи, бинокли и гранатомёты исключены.";return r;}
    r.fireModes=SimulationFireModes(q.weapon);
    warn(r,"Неподвижный ствол; точность владельца = 0. Отдача камеры, движение, прицел и обвесы не добавляются к разбросу.");
    warn(r,"Расстреливается полный магазин; осечки показаны как вероятность, но не прерывают опыт. Ресурс до состояния 0 не является запретом дальнейших выстрелов в движке.");
    if(q.fireMode!=-1)warn(r,"Между нажатиями спуска добавлена заданная лабораторная пауза. Время перезарядки и анимации не включены.");
    spread(q,r);trajectory(q,r);
    if(!r.spreadValid&&!r.trajectoryValid)r.error="Расчёт недоступен: проверьте предупреждения и параметры конфигов.";
    return r;
}
SimulationParameterInfo DescribeSimulationParameter(const std::string& key){
    struct Row {const char* key;const char* title;const char* description;const char* effect;};
    static const Row rows[]={
        {"fire_dispersion_base","Базовый разброс, градусы","Половина угла конуса: переводится в радианы и умножается на k_disp патрона и множитель состояния.","Больше → шире круг попаданий. Радиус границы = дистанция × tan(угол)."},
        {"fire_dispersion_condition_factor","Ухудшение точности при износе","Множитель = 1 + параметр × (1 − состояние). Износ данного выстрела уже учтён.","Больше → сильнее расширение конуса у изношенного оружия."},
        {"condition_shot_dec","Износ за патрон","Базовая потеря состояния × impair патрона. При превышении preffered_fire_mode дополнительно × номер выстрела в очереди.","Больше → меньше выстрелов до состояния 0%; дробины не умножают износ."},
        {"rpm","Темп стрельбы, выстр./мин","Минимальный интервал между выстрелами = 60 / RPM секунд.","Больше → быстрее расход магазина. Сам по себе RPM не меняет конус фиксированного ствола."},
        {"ammo_mag_size","Ёмкость магазина","Число патронов в одном полном лабораторном магазине.","Больше → больше точек, больше суммарный износ и дольше серия."},
        {"ammo_elapsed","Начальная зарядка","Начальное число патронов в игровом объекте.","На стенде всегда полный ammo_mag_size, поэтому этот параметр не меняет опыт."},
        {"fire_modes","Режимы стрельбы","1 — одиночный, положительное N — очередь N, −1 — автоматический. Пистолетные классы, СВД/СВУ и дробовики принудительно одиночные в master.","Режим меняет границы очередей и время; при preferred-штрафе также износ."},
        {"preffered_fire_mode","Предпочтительный режим","Именно такое написание ключа читает master. −1 отключает дополнительный износ; превышение включает множитель номера выстрела.","Например, очередь 3 при предпочтительном 2 теряет базовый износ × (1+2+3)."},
        {"ammo_class","Совместимые патроны","Список секций боеприпасов. Стенд читает выбранный патрон из weapons.ltx и включённых LTX.","Патрон меняет k_disp, impair, buck_shot и k_dist согласно исходникам."},
        {"bullet_speed","Начальная скорость, м/с","Начальная скорость пули из CShootingObject. В этой ветке у патрона нет коэффициента k_speed.","Больше → меньше время полёта и обычно меньше падение на той же дистанции."},
        {"fire_distance","Предельный путь, м","Пуля удаляется после пройденного пути fire_distance × k_dist; это длина траектории, а не только горизонтальная дальность.","Меньше → пуля может исчезнуть до выбранной отметки 100–300 м."},
        {"misfire_probability","Базовая вероятность осечки","При состоянии >0.95 вероятность 0; иначе clamp(база + (1−состояние)^3 × misfire_condition_k, 0, 0.99). Проверка после выстрела.","Изменяет показанную вероятность; лабораторная серия не прерывается случайными осечками."},
        {"misfire_condition_k","Рост осечек от износа","Коэффициент кубического роста вероятности осечки; при отсутствии равен 1 в исходниках.","Больше → выше вероятность задержки при низком состоянии, но не дополнительный износ."},
        {"use_aim_bullet","Флаг прицельной пули","Выставляется после расчёта random_dir и передаётся в BulletManager.","Не уменьшает разброс и не меняет свободную траекторию в master; здесь без эффекта."},
        {"time_to_aim","Пауза для прицельной пули, с","Порог времени между выстрелами для флага aim_bullet.","Не множитель точности; в фиксированном конусе и свободной траектории не используется."},
        {"dispersion_start","Начало изменения направления","Первые N выстрелов берут стартовое направление/позицию, последующие — текущие параметры стрельбы владельца.","При неподвижном стволе направления совпадают; этот стенд не добавляет отдачу камеры."},
        {"hit_power","Сила попадания","Сила выбирается по сложности, умножается на k_hit патрона; при попадании применяется speed/max_speed.","Влияет на урон при столкновении, не на конус и свободную траекторию. Мишень здесь без материала."},
        {"hit_impulse","Импульс попадания","Умножается на k_impulse и поправку скорости при столкновении.","Не изменяет скорость полёта: это параметр передачи импульса объекту при попадании."},
        {"cam_dispersion","Отдача камеры за выстрел","Угловой шаг камеры; алгоритм эффектора владельца отдельный от GetFireDispersion.","Фиксированный ствол исключает отдачу камеры. Конус от этого ползунка не меняется."},
        {"cam_dispersion_inc","Рост отдачи в очереди","Прибавка к углу камеры по номеру выстрела.","Не включено в фиксированный стенд; не следует считать ростом собственного разброса оружия."},
        {"cam_relax_speed","Скорость возврата камеры","Параметр возврата shot-effector, зависящий от состояния активности и обработчика камеры.","Фиксированный ствол исключает движение камеры; распределение не меняется."},
        {"cam_max_angle","Ограничение вертикальной отдачи","Предельный вертикальный угол shot-effector камеры.","Без эффекта на неподвижном стенде."},
        {"cam_max_angle_horz","Ограничение горизонтальной отдачи","Предельный горизонтальный угол shot-effector камеры.","Без эффекта на неподвижном стенде."},
        {"cam_step_angle_horz","Горизонтальный шаг отдачи","Случайный горизонтальный шаг камеры при выстреле.","Без эффекта на неподвижном стенде."},
        {"cam_dispersion_frac","Доля детерминированной отдачи","Смешивает основной и случайный вклад угла shot-effector.","Без эффекта на неподвижном стенде."},
        {"k_disp","Разброс патрона","Множитель собственной дисперсии оружия.","Больше → шире круг попаданий."},
        {"buck_shot","Число дробин","Количество вызовов FireBullet на один патрон.","Больше → больше попаданий; износ и расход патронов считаются один раз."},
        {"impair","Износ от патрона","Множитель потери состояния на один патрон.","Больше → меньше выстрелов до состояния 0%."},
        {"k_dist","Дальность патрона","Множитель максимального пути, не начальной скорости.","Меняет момент удаления пули по дальности."},
        {"k_air_resistance","Сопротивление патрона","В CalcBullet используется только в мультиплеере. Одиночная игра берёт глобальный air_resistance_k.","На одиночном стенде не влияет."},
        {"gravity_const","Гравитация движка","Берётся из [bullet_manager]; скорость Y уменьшается на g × time_step.","Больше → сильнее падение траектории."},
        {"air_resistance_k","Сопротивление воздуха SP","Каждый шаг скорость умножается на (1−k × dt), затем добавляется гравитация.","Больше → ниже скорость и больше время полёта."},
        {"time_step","Шаг BulletManager, мс","Положение сначала изменяется со старой скоростью, затем обновляется скорость.","Меняет дискретизацию траектории; непрерывная парабола не является этим алгоритмом."},
        {"min_bullet_speed","Порог удаления пули, м/с","После обновления скорости пуля удаляется при скорости ниже порога.","Больше → пуля может прекратить полёт раньше."}
    };
    for(const auto& r:rows)if(key==r.key)return {true,r.title,r.description,r.effect};
    if(key.rfind("PDM_",0)==0||key.rfind("pdm_",0)==0)return {true,"Поправка точности владельца","Параметр точности стрелка при движении/позе. На стенде owner accuracy = 0.","Здесь не меняет круг: поза, движение, экипировка и ввод игрока исключены."};
    if(key.rfind("zoom_cam_",0)==0||key.rfind("cam_",0)==0)return {true,"Параметр камеры/отдачи","Относится к камере или эффектору отдачи, а не к собственной дисперсии ствола.","На неподвижном стенде не применяется; точность в реальной игре зависит от владельца."};
    if(key.rfind("silencer_",0)==0)return {true,"Параметр глушителя","Применяется движком при установленном совместимом глушителе.","Стенд рассчитывает оружие без обвесов; этот параметр не меняет опыт."};
    return {};
}
}
