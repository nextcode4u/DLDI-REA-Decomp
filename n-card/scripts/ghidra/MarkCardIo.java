import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.*;
import ghidra.program.model.mem.MemoryBlock;

public class MarkCardIo extends GhidraScript {
    private void reg(long offset, String name, DataType type) throws Exception {
        Address address = toAddr(offset);
        createData(address, type);
        createLabel(address, name, true);
    }
    @Override
    public void run() throws Exception {
        MemoryBlock card = currentProgram.getMemory().createUninitializedBlock(
                "card_io", toAddr(0x04000000), 0x2000, false);
        card.setRead(true);
        card.setWrite(true);
        card.setVolatile(true);
        MemoryBlock data = currentProgram.getMemory().createUninitializedBlock(
                "card_fifo", toAddr(0x04100010), 4, false);
        data.setRead(true);
        data.setWrite(true);
        data.setVolatile(true);
        reg(0x040001a1, "CARD_CR1H", ByteDataType.dataType);
        reg(0x040001a4, "CARD_CR2", DWordDataType.dataType);
        reg(0x040001a8, "CARD_COMMAND", new ArrayDataType(ByteDataType.dataType, 8, 1));
        reg(0x04100010, "CARD_DATA_RD", DWordDataType.dataType);
    }
}
