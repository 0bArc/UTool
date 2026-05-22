using System.Text.Json.Nodes;

namespace UTool.Infrastructure.PlayerData;

public static class AccoladeQuery
{
    public static bool HasCompletedAccolade(string accoladesJson, string rowName, string? dataTableName = "D_Accolades")
    {
        var root = JsonNode.Parse(accoladesJson);
        if (root is null)
            return false;

        if (root["CompletedAccolades"] is not JsonArray completed)
            return false;

        foreach (var entry in completed)
        {
            if (entry?["Accolade"] is not JsonObject accolade)
                continue;

            var row = accolade["RowName"]?.GetValue<string>();
            if (!string.Equals(row, rowName, StringComparison.Ordinal))
                continue;

            if (dataTableName is not null)
            {
                var table = accolade["DataTableName"]?.GetValue<string>();
                if (!string.Equals(table, dataTableName, StringComparison.Ordinal))
                    continue;
            }

            var time = entry["TimeCompleted"]?.GetValue<string>();
            if (!string.IsNullOrWhiteSpace(time))
                return true;
        }

        return false;
    }
}
