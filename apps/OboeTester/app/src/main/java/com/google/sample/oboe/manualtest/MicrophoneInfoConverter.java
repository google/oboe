package com.google.sample.oboe.manualtest;

import android.media.MicrophoneInfo;

import java.io.IOException;
import java.util.List;

public class MicrophoneInfoConverter {

    static String convertDirectionality(int directionality) {
        switch(directionality) {
            case MicrophoneInfo.DIRECTIONALITY_BI_DIRECTIONAL:
                return "Bidirectional";
            case MicrophoneInfo.DIRECTIONALITY_OMNI:
                return "Omni";
            case MicrophoneInfo.DIRECTIONALITY_CARDIOID:
                return "Cardioid";
            case MicrophoneInfo.DIRECTIONALITY_SUPER_CARDIOID:
                return "SuperCardioid";
            case MicrophoneInfo.DIRECTIONALITY_HYPER_CARDIOID:
                return "HyperCardioid";
            default:
                return "Unknown";
        }
    }

    static String convertLocation(int location) {
        switch(location) {
            case MicrophoneInfo.LOCATION_MAINBODY:
                return "Main Body";
            case MicrophoneInfo.LOCATION_MAINBODY_MOVABLE:
                return "Main Body Movable";
            case MicrophoneInfo.LOCATION_PERIPHERAL:
                return "Peripheral";
            default:
                return "Unknown";
        }
    }

    static String convertPosition(MicrophoneInfo.Coordinate3F position) {
        if (position == MicrophoneInfo.POSITION_UNKNOWN) return "Unknown";
        return String.format("{ %6.4g, %5.3g, %5.3g }",
                position.x, position.y, position.z);
    }

    public static String reportMicrophoneInfo(MicrophoneInfo micInfo) {
        StringBuffer sb = new StringBuffer();
        sb.append("\n==== Microphone ========= " + micInfo.getId());
        sb.append("\nAddress    : " + micInfo.getAddress());
        sb.append("\nDescription: " + micInfo.getDescription());
        sb.append("\nDirection  : "+ convertDirectionality(micInfo.getDirectionality()));
        sb.append("\nLocation   : "+ convertLocation(micInfo.getLocation()));
        sb.append("\nGroup      : " + micInfo.getGroup());
        sb.append("\nIndexInTheGroup: " + micInfo.getIndexInTheGroup());
        sb.append("\nPosition   : "+ convertPosition(micInfo.getPosition()));
        sb.append("\nType       : "+ micInfo.getType());

        sb.append("\n");
        return sb.toString();
    }

}
