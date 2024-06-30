#include <new>
#include <codecvt>
#include <locale>

#include <BNM/BasicMonoStructures.hpp>
#include "Internals.hpp"

typedef std::basic_string<BNM::IL2CPP::Il2CppChar> string16;

std::string Utf16ToUtf8(BNM::IL2CPP::Il2CppChar *utf16String, size_t length) {
    std::u16string u16str(utf16String, utf16String + length);
    std::wstring_convert<std::codecvt_utf8_utf16<BNM::IL2CPP::Il2CppChar>, BNM::IL2CPP::Il2CppChar> convert;
    return convert.to_bytes(u16str);
}

string16 Utf8ToUtf16(const char *utf8String, size_t length) {
    std::string u8str(utf8String, utf8String + length);
    std::wstring_convert<std::codecvt_utf8_utf16<BNM::IL2CPP::Il2CppChar>, BNM::IL2CPP::Il2CppChar> convert;
    return convert.from_bytes(u8str);
}

using namespace BNM::Structures::Mono;

std::string String::str() {
    BNM_CHECK_SELF(DBG_BNM_MSG_String_SelfCheck_Error);
    if (!length) return {};
    return Utf16ToUtf8(chars, length);
}

unsigned int String::GetHash() const {
    BNM_CHECK_SELF(0);
    const IL2CPP::Il2CppChar *p = chars;
    unsigned int h = 0;
    for (int i = 0; i < length; ++i) {
        h = (h << 5) - h + *p;
        p++;
    }
    return h;
}

// Создать обычную C#-строку, как это делает il2cpp
String *String::Create(const char *str) {
    const size_t length = strlen(str);
    const size_t utf16Size = sizeof(IL2CPP::Il2CppChar) * length;
    auto ret = (String *) malloc(sizeof(String) + utf16Size);
    memset(ret, 0, sizeof(String) + utf16Size);
    ret->length = (int) length;
    auto u16 = Utf8ToUtf16(str, ret->length);
    memcpy(ret->chars, u16.data(), utf16Size);
    auto empty = Empty();
    if (empty) ret->klass = empty->klass;
    return (String *) ret;
}

String *String::Create(const std::string &str) { return Create(str.c_str()); }

String *String::Empty() {
    return Internal::vmData.String$$Empty ? *Internal::vmData.String$$Empty : nullptr;
}

#ifdef BNM_ALLOW_SELF_CHECKS

bool String::SelfCheck() const {
    if (std::launder(this)) return true;
    BNM_LOG_ERR("[String::SelfCheck] Попытка использовать мёртвую строку!");
    return false;
}

#endif

void String::Destroy() {
    length = 0;
    free(this);
}

void *PRIVATE_MonoListData::CompareExchange4List(
        void *syncRoot) { // Единственный нормальный способ вызвать CompareExchange для syncRoot для List
    if (Internal::vmData.Interlocked$$CompareExchange)
        Internal::vmData.Interlocked$$CompareExchange((void **) &syncRoot,
                                                      (void *) Internal::vmData.Object.CreateNewInstance(),
                                                      (void *) nullptr);
    return syncRoot;
}

// Метод для получения и создания типов для каждого типа списков
BNM::IL2CPP::Il2CppClass *
BNM::Structures::Mono::PRIVATE_MonoListData::TryGetMonoListClass(uint32_t typeHash,
                                                                 std::array<PRIVATE_MonoListData::MethodData, 16> &data) {
    auto &klass = Internal::customListsMap[typeHash];
    if (klass) return klass;

    std::map<size_t, BNM::IL2CPP::MethodInfo *> createdMethods{};
    auto templateClass = Internal::customListTemplateClass.GetClass();
    auto size = sizeof(IL2CPP::Il2CppClass) +
                templateClass->vtable_count * sizeof(IL2CPP::VirtualInvokeData);
    auto typedClass = (IL2CPP::Il2CppClass *) malloc(size);
    memcpy(typedClass, templateClass, size);
    auto hash = std::hash<std::string_view>();
    for (uint16_t i = 4 /* Пропуск virtual методов от System.Object */;
         i < typedClass->vtable_count; ++i) {
        auto &cur = typedClass->vtable[i];
        auto name = std::string_view(cur.method->name);
        auto dot = name.rfind('.');
        if (dot != std::string_view::npos) name = name.substr(dot + 1);

        auto iterator = data.begin();
        for (; iterator != data.end(); ++iterator) if (iterator->methodName == name) break;

        auto &methodInfo = createdMethods[hash(name)];

        if (methodInfo == nullptr) {
            methodInfo = (IL2CPP::MethodInfo *) malloc(sizeof(IL2CPP::MethodInfo));
            memcpy((void *) methodInfo, (void *) cur.method, sizeof(IL2CPP::MethodInfo));
            methodInfo->methodPointer = (decltype(methodInfo->methodPointer)) iterator->ptr;
        }
        cur.method = methodInfo;
        cur.methodPtr = methodInfo->methodPointer;
    }
    klass = typedClass;
    return klass;
}