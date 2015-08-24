/// "type" : { "toString" : "myStruct" }
struct myStruct {};

/// "type" : { "toString" : "myClass" }
class myClass {};

/// "type" : { "toString" : "myUnion" }
union myUnion {};

/// "type" : { "toString" : "myEnum" }
enum myEnum {
    /// "type" : { "toString" : "myEnum::myEnumerator0", "plainValue" : "0" }
    myEnumerator0,
    /// "type" : { "toString" : "myEnum::myEnumerator1", "plainValue" : "1" }
    myEnumerator1
};

/// "type" : { "toString" : "myTypedef" },
/// "unaliasedType" : { "toString" : "int"},
/// "kind" : "Type"
typedef int myTypedef;

/// "type" : { "toString" : "myTypeAlias" },
/// "unaliasedType" : { "toString" : "int"},
/// "kind" : "Type"
using myTypeAlias = int;

class Friend;
class Class
{
    /// "type" : { "toString" : "Friend", "EXPECT_FAIL": {"toString": "FriendDecl is not accessible through LibClang"} }
    friend class Friend;
};

/// "toString" : "int main (int, char**)"
int main(int argc, char** argv)
{
    /// "toString" : "short int s"
    short s;
    /// "toString" : "int a"
    int a;
    /// "toString" : "const float b"
    const float b = 0;
    /// "toString" : "volatile long long int c"
    volatile long long c;
    /// "toString" : "void* v_ptr"
    void* v_ptr;
    /// "toString" : "void* const* v_ptr2"
    void* const* v_ptr2;
    /// "toString" : "int[5] arr"
    int arr[5];
    /// "toString" : "int[] arr2"
    int arr2[argc];
    /// "toString" : "int[] arr3"
    int arr3[] = {};
    enum { Arr4Size = 5 };
    /// "toString" : "int[5] arr4"
    int arr4[Arr4Size];
    /// "toString" : "unsigned int uint"
    unsigned int uint;
    /// "toString" : "long unsigned int ulong"
    unsigned long ulong;
    /// "toString" : "long long unsigned int ulonglong"
    unsigned long long ulonglong;
    /// "toString" : "short unsigned int ushort"
    unsigned short ushort;
    /// "toString" : "const int& a_lref"
    const int& a_lref = a;
    /// "toString" : "int&& a_rref"
    int&& a_rref = a + a;
    /// "toString" : "char c1"
    char c1;
    /// "toString" : "unsigned char c2"
    unsigned char c2;
    /// "toString" : "signed char c3"
    signed char c3;
    /// "toString" : "wchar_t wc"
    wchar_t wc;
    /// "toString" : "myStruct myS"
    myStruct myS;
    /// "toString" : "myClass myC"
    myClass myC;
    /// "toString" : "myUnion myU"
    myUnion myU;
    /// "toString" : "myEnum myE"
    myEnum myE;
    /// "toString" : "myTypedef myT"
    myTypedef myT;
    /// "toString" : "__int128 i128"
    __int128 i128;
    /// "toString" : "unsigned __int128 ui128"
    unsigned __int128 ui128;
    // TODO: get the actual type here somehow?
    /// "toString" : "auto autoVar"
    auto autoVar = 123;
    /// "toString" : "const volatile auto autoVar2"
    const volatile auto autoVar2 = 321;
}
