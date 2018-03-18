@0xdde41f3d290d408e;

struct String {
    string @0 :Text;
}

struct ProfilePin {
    dutId @0 :Int32;
    pinName @1 :Text;
    compName @2 :Text;
    netName @3 :Text;
    netAlias @4 :Text;
    tag @5 :ProfileTags;
    tagData @6 :Int32;
    dutIoId @7 :Int32;
    numDestPinNames @8 :UInt32;
    destPinNames @9 :List(String);

    enum ProfileTags {
        profileTagNone @0;
        profileTagCclk @1;
        profileTagResetB @2;
        profileTagCsiB @3;
        profileTagRdwrB @4;
        profileTagProgramB @5;
        profileTagInitB @6;
        profileTagDone @7;
        profileTagData @8;
        profileTagGpio @9;
    }
}

struct VecChunk {
    id @0 :UInt8;
    numVecs @1 :UInt32;
    vecData @2 :Data;
    vecDataSize @3 :UInt32;
    vecDataCompressedSize @4 :UInt32;
}

struct SerialStim {
    type @0 :StimTypes;
    numPins @1 :UInt16;
    pins @2 :List(ProfilePin);
    numVecs @3 :UInt32;
    numUnrolledVecs @4 :UInt32;
    numVecChunks @5 :UInt32;
    vecChunks @6 :List(VecChunk);

    enum StimTypes {
        stimTypeNone @0;
        stimTypeRbt @1;
        stimTypeBin @2;
        stimTypeBit @3;
        stimTypeDots @4;
        stimTypeRaw @5;
    }

}









