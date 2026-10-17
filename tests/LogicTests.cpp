#include "PCH.h"
#include "ActionRules.h"
#include "DataHandler.h"
#include "NativeValidation.h"
#include "Hook_AttackStart.h"
#include "DebugAPI/Util.h"
#include <spdlog/sinks/ostream_sink.h>

static unsigned checks{};
#define CHECK(value) do { ++checks; if (!(value)) { std::cerr << "FAIL line " << __LINE__ << '\n'; return 1; } } while (false)

int main()
{
    std::ostringstream diagnostics;
    struct RestoreLogger {
        std::shared_ptr<spdlog::logger> previous;
        ~RestoreLogger() { spdlog::set_default_logger(std::move(previous)); }
    } restore{spdlog::default_logger()};
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("scar-tests",
        std::make_shared<spdlog::sinks::ostream_sink_mt>(diagnostics)));
    using namespace SCAR;
    using json=nlohmann::json;
    CHECK(ActionRules::Type("lpa")=="LPA");
    CHECK(ActionRules::Type("Idle")=="IDLE");
    CHECK(ActionRules::DebugFlag("TRUE")==true);
    CHECK(ActionRules::DebugFlag("false")==false);
    CHECK(ActionRules::DebugFlag("1")==true);
    CHECK(ActionRules::DebugFlag("0")==false);
    CHECK(!ActionRules::DebugFlag("abc"));
    CHECK(!ActionRules::DebugFlag(""));
    CHECK(ActionRules::ComboChance("")==100.f);
    CHECK(ActionRules::ComboChance("  ")==100.f);
    CHECK(ActionRules::ComboChance(" +50.5 ")==50.5f);
    CHECK(ActionRules::ComboChance("-1")==0.f);
    CHECK(ActionRules::ComboChance("200")==100.f);
    for (const auto text : {"abc","50junk","nan","inf","1e999","+","+-5"}) CHECK(!ActionRules::ComboChance(text));
    for (int i=0;i<=100;++i) {
        CHECK(!ActionRules::ChancePasses(0.f,static_cast<float>(i)));
        CHECK(ActionRules::ChancePasses(100.f,static_cast<float>(i)));
        CHECK(ActionRules::ChancePasses(50.f,static_cast<float>(i))==(i<=50));
    }
    CHECK(!ActionRules::ChancePasses(std::numeric_limits<float>::quiet_NaN(),1));
    CHECK(ActionRules::WithinAngle(1,1,2));
    CHECK(ActionRules::WithinAngle(2,1,2));
    CHECK(ActionRules::WithinAngle(3,2,-2));
    CHECK(ActionRules::WithinAngle(-3,2,-2));
    CHECK(ActionRules::WithinAngle(2,2,-2));
    CHECK(ActionRules::WithinAngle(-2,2,-2));
    CHECK(!ActionRules::WithinAngle(0,2,-2));
    CHECK(ActionRules::WithinAngle(0,0,0));
    CHECK(ActionRules::Number(json{{"x",.5}},"x")==.5f);
    bool rejected{};
    try { ActionRules::Number(json{{"x",std::numeric_limits<double>::infinity()}},"x"); } catch(const std::invalid_argument&) { rejected=true; }
    CHECK(rejected);
    json action{{"IdleAnimation","AttackStart"},{"MinDistance",0},{"MaxDistance",150},{"StartAngle",-60},{"EndAngle",60},{"Chance",100},{"Type","lpa"}};
    CHECK(!action.get<SCARActionData>().PerformSCARAction(nullptr,nullptr));
    for (const auto key : {"MinDistance","MaxDistance","StartAngle","EndAngle","Chance","WeaponLength"}) {
        auto invalid=action; invalid[key]=std::numeric_limits<double>::infinity(); rejected=false;
        try { (void)invalid.get<SCARActionData>(); } catch(const std::invalid_argument&) { rejected=true; }
        CHECK(rejected);
    }
    const auto annotation="SCAR_ActionData"+action.dump();
    CHECK(DataHandler::ParseAnnotation(annotation,0.f).has_value());
    CHECK(DataHandler::ParseAnnotation(annotation,-1.f).has_value());
    CHECK(!DataHandler::ParseAnnotation(annotation,std::numeric_limits<float>::infinity()));
    CHECK(!DataHandler::ParseAnnotation(annotation,std::numeric_limits<float>::quiet_NaN()));
    for (const auto text : {"", "OtherEvent", "SCAR_ActionData", "SCAR_ActionData{", "SCAR_ActionData{}",
         "SCAR_ActionData[]", "SCAR_ActionDatanull", "SCAR_ActionDatafalse", "SCAR_ActionData0"}) {
        CHECK(!DataHandler::ParseAnnotation(text,0.f));
    }
    CHECK(!DataHandler::ParseAnnotation(annotation+"junk",0.f));
    for (const auto key : {"IdleAnimation","MinDistance","MaxDistance","StartAngle","EndAngle","Chance","Type"}) {
        auto invalid=action; invalid.erase(key);
        CHECK(!DataHandler::ParseAnnotation("SCAR_ActionData"+invalid.dump(),0.f));
        invalid=action; invalid[key]=nullptr;
        CHECK(!DataHandler::ParseAnnotation("SCAR_ActionData"+invalid.dump(),0.f));
        invalid=action; invalid[key]=json::array();
        CHECK(!DataHandler::ParseAnnotation("SCAR_ActionData"+invalid.dump(),0.f));
    }
    auto invalid=action; invalid["IdleAnimation"]="";
    CHECK(!DataHandler::ParseAnnotation("SCAR_ActionData"+invalid.dump(),0.f));
    for (const auto type : {"RA","RPA","LA","LPA","DA","DPA","BA","BPA","IDLE","ra","lpa","idle"}) {
        auto valid=action; valid["Type"]=type; valid["WeaponLength"]=35.f;
        CHECK(DataHandler::ParseAnnotation("SCAR_ActionData"+valid.dump(),1.f).has_value());
    }
    auto lower=DataHandler::ParseAnnotation(annotation,1.f);
    auto higher=DataHandler::ParseAnnotation(annotation,2.f);
    CHECK(SCARActionData::SortByWeight(*higher,*lower));
    CHECK(!SCARActionData::SortByWeight(*lower,*higher));
    CHECK(!SCARActionData::SortByWeight(*lower,*lower));
    std::vector<SCARActionData> parsed;
    for (const auto& text : {std::string("Unrelated"),std::string("SCAR_ActionData{"),annotation,std::string("SCAR_ActionData[]"),annotation}) {
        if (auto entry=DataHandler::ParseAnnotation(text,3.f)) parsed.push_back(std::move(*entry));
    }
    CHECK(parsed.size()==2);
    CHECK(DataHandler::GetSCARDataClip(nullptr,&parsed)==nullptr);
    CHECK(parsed.empty());
    CHECK(DataHandler::GetSCARDataClip(nullptr)==nullptr);
    CHECK(DataHandler::GetSCARActionData(nullptr).empty());
    CHECK(diagnostics.str().find("Invalid SCAR annotation JSON; action ignored")!=std::string::npos);
    CHECK(diagnostics.str().find("Invalid SCAR annotation value; action ignored")!=std::string::npos);
    RE::NiMatrix3 identity;
    const auto angles=Util::RotMatrixToEuler(identity);
    CHECK(std::abs(angles.x)<1e-5f && std::abs(angles.y)<1e-5f && std::abs(angles.z)<1e-5f);
    identity.MakeZRotation(std::numbers::pi_v<float>/2);
    const auto rotated=Util::RotMatrixToEuler(identity);
    // NiMatrix3::MakeZRotation uses the opposite sign to GLM's Euler angle.
    CHECK(std::abs(rotated.z+std::numbers::pi_v<float>/2)<1e-5f);
    for (const auto matrix : {RE::NiMatrix3(.3f,0.f,0.f),RE::NiMatrix3(0.f,.4f,0.f),RE::NiMatrix3(0.f,0.f,.5f),RE::NiMatrix3(.2f,.3f,.4f)}) {
        const auto euler=Util::RotMatrixToEuler(matrix);
        for (const auto point : {RE::NiPoint3(1.f,0.f,0.f),RE::NiPoint3(0.f,1.f,0.f),RE::NiPoint3(0.f,0.f,1.f)}) {
            const auto expected=matrix*point;
            const auto actual=Util::RotateVector(euler,Util::NiPointToVec(point));
            CHECK(std::abs(actual.x-expected.x)<1e-4f);
            CHECK(std::abs(actual.y-expected.y)<1e-4f);
            CHECK(std::abs(actual.z-expected.z)<1e-4f);
        }
    }
    std::ifstream source("build/preflight/native-fixtures.json"); const auto native=json::parse(source);
    const auto bytes=[](const std::string& hex) { std::vector<std::uint8_t> b; for(std::size_t i=0;i<hex.size();i+=2)b.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i,2),nullptr,16))); return b; };
    for(const auto& call:native.at("calls")) {
        const auto& function=native.at(call.at("function").get<std::string>());
        auto code=bytes(function.at("code")); const auto base=function.at("base").get<std::uintptr_t>();
        const auto offset=call.at("offset").get<std::size_t>(); const auto target=call.at("target").get<std::uintptr_t>();
        CHECK(NativeValidation::Call(code,base,offset,target));
        CHECK(!NativeValidation::Call(code,base,offset+1,target));
        CHECK(!NativeValidation::Call(code,base,offset,target+1));
        code[offset]=0x90; CHECK(!NativeValidation::Call(code,base,offset,target));
    }
    CHECK(!NativeValidation::Call(std::array<std::uint8_t,1>{0xE8},0,0,0));
    const auto& prolog=AttackDistancHook::Prolog(true,true);
    CHECK(prolog.Size==27 && AttackDistancHook::TrampolineBytes==58);
    CHECK(std::memcmp(reinterpret_cast<const std::uint8_t*>(prolog.Data)+11,"\x4C\x8B\x85\xD0\x00\x00\x00",7)==0);
    CHECK(AttackDistancHook::Prolog(true,false).Size==25 && AttackDistancHook::Prolog(false,false).Size==25);
    CHECK(AttackDistancHook::Epilog(true).Size==4 && AttackDistancHook::Epilog(false).Size==4);
    std::cout << "PASS: " << checks << " SCAR logic/native regression checks\n";
}
