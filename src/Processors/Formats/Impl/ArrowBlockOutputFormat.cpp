#include "ArrowBlockOutputFormat.h"

#if USE_ARROW

#include <Formats/FormatFactory.h>
#include <arrow/ipc/writer.h>
#include <arrow/table.h>
#include "ArrowBufferedStreams.h"
#include "CHColumnToArrowColumn.h"

namespace DB
{
namespace ErrorCodes
{
    extern const int UNKNOWN_EXCEPTION;
}

ArrowBlockOutputFormat::ArrowBlockOutputFormat(WriteBuffer & out_, const Block & header_, const FormatSettings & format_settings_)
    : IOutputFormat(header_, out_), format_settings{format_settings_}, arrow_ostream{std::make_shared<ArrowBufferedOutputStream>(out_)}
{
}

void ArrowBlockOutputFormat::consume(Chunk chunk)
{
    const Block & header = getPort(PortKind::Main).getHeader();
    const size_t columns_num = chunk.getNumColumns();
    std::shared_ptr<arrow::Table> arrow_table;

    CHColumnToArrowColumn::chChunkToArrowTable(arrow_table, header, chunk, columns_num, "Arrow");

    if (!writer)
    {
        // TODO: should we use arrow::ipc::IpcOptions::alignment?
        auto status = arrow::ipc::RecordBatchFileWriter::Open(arrow_ostream.get(), arrow_table->schema(), &writer);
        if (!status.ok())
            throw Exception{"Error while opening a table: " + status.ToString(), ErrorCodes::UNKNOWN_EXCEPTION};
    }

    // TODO: calculate row_group_size depending on a number of rows and table size
    auto status = writer->WriteTable(*arrow_table, format_settings.arrow.row_group_size);

    if (!status.ok())
        throw Exception{"Error while writing a table: " + status.ToString(), ErrorCodes::UNKNOWN_EXCEPTION};
}

void ArrowBlockOutputFormat::finalize()
{
    if (writer)
    {
        auto status = writer->Close();
        if (!status.ok())
            throw Exception{"Error while closing a table: " + status.ToString(), ErrorCodes::UNKNOWN_EXCEPTION};
    }
}

void registerOutputFormatProcessorArrow(FormatFactory & factory)
{
    factory.registerOutputFormatProcessor(
        "Arrow",
        [](WriteBuffer & buf,
           const Block & sample,
           FormatFactory::WriteCallback,
           const FormatSettings & format_settings)
        {
            return std::make_shared<ArrowBlockOutputFormat>(buf, sample, format_settings);
        });
}

}

#else

namespace DB
{
class FormatFactory;
void registerOutputFormatProcessorArrow(FormatFactory &)
{
}
}

#endif
