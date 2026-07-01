#include "key.h"
#include "blue.h"
#include "_key.h"

int _key_key_mast(PikaObj *self, char* keyname, pika_float State)
{
    (void)self;
    (void)keyname;
    (void)State;
    return 0.0f;
}

int _key_key_remote(PikaObj *self, char* keys, char* coor)
{
   uint8_t keyValue = 0;  
   
    if (keys == NULL || coor == NULL) {
        return 0;
    }

		DEV_BLUE *blue = read_blue((SensorBase *)getHubBase(8));
		if(blue == NULL)
				return 0;
		
		uint8_t *remote  = blue->remoteValue;
		
 static const char* RemoteKeyValue[] = {
       "up","down","left","right","Y","A","B","X","L1","R1"
    };

    static int keyIndexMap[256] = {0}; // ʹ��ASCIIֵ��Ϊ����
    static bool mapInitialized = false;
    
    if (!mapInitialized) {
        for (int i = 0; i < 10; i++) {
            if (RemoteKeyValue[i] != NULL) {
                // ʹ�ü����ĵ�һ���ַ������򵥹�ϣ
                keyIndexMap[(uint8_t)RemoteKeyValue[i][0]] = i + 1;
            }
        }
        mapInitialized = true;
    }

    int keyIndex = -1;
    if (keys[0] != '\0') {
        int mapIndex = keyIndexMap[(uint8_t)keys[0]];
        if (mapIndex > 0 && strcmp(RemoteKeyValue[mapIndex - 1], keys) == 0) {
            keyIndex = mapIndex - 1;
        }
    }

    if (keyIndex == -1) {
        for (int i = 0; i < 10; i++) {
            if (strcmp(RemoteKeyValue[i], keys) == 0) {
                keyIndex = i;
                break;
            }
        }
    }
    
 
    if (keyIndex != -1) {
        pika_GIL_EXIT();
        keyValue = remote[keyIndex];
        pika_GIL_ENTER();
    }

    if (strcmp(coor, "press") == 0) {
        return (keyValue == 1) ? 1 : 0;
    }
    else if (strcmp(coor, "unpress") == 0) {
        return (keyValue == 0) ? 1 : 0;
    }
    
    return 0;		
}

int _key_read_adcance_left_offset(PikaObj *self)
{
    (void)self;
    return read_advance_offset1();
}

int _key_read_advance_right_offset(PikaObj *self)
{
    (void)self;
    return read_advance_offset2();
}

int _key_read_retreat_left_offset(PikaObj *self)
{
    (void)self;
    return read_retreat_offset1();
}

int _key_read_retreat_right_offset(PikaObj *self)
{
    (void)self;
    return read_retreat_offset2();
}
