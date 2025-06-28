package bgu.spl.net.api;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

public class MessageEncoderDecoderImpl implements MessageEncoderDecoder<String> {

    private static final byte NULL_TERMINATOR = '\u0000';
    private byte[] buffer = new byte[1024];
    private int length = 0;
    
    @Override
    public String decodeNextByte(byte nextByte) {
        if (nextByte == NULL_TERMINATOR) {
            // Message is complete
            String message = new String(buffer, 0, length, StandardCharsets.UTF_8);
            Arrays.fill(buffer, 0, length, (byte) 0);
            length = 0; // Reset buffer
            return message;
        } else {
            // Add byte to buffer
            if (length >= buffer.length) {
                buffer = Arrays.copyOf(buffer, buffer.length * 2); // Expand buffer if necessary
            }
            buffer[length++] = nextByte;
            return null; // Message not complete yet
        }
    }

    @Override
    public byte[] encode(String message) {
        return (message + "\u0000").getBytes(StandardCharsets.UTF_8);
    }
}

