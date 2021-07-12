using System;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Azure.WebJobs;
using Microsoft.Azure.WebJobs.Extensions.Http;
using Microsoft.Extensions.Logging;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;


// https://docs.microsoft.com/en-us/rest/api/iotcentral/devices/getproperties


namespace Glovebox.Function
{
    public class IoTCentralPatch
    {
        public int DesiredChannelId { get; set; }
        public string DeviceId { get; set; }
    }

    public class ChannelIdResponse
    {
        public int DesiredChannelId { get; set; }
    }

    public static class GetChannelId
    {
        private static string authorization = Environment.GetEnvironmentVariable("IOT_CENTRAL_API_TOKEN");

        private static string iotCentralUrl = Environment.GetEnvironmentVariable("IOT_CENTRAL_URL");


        [FunctionName("getchannelid")]
        public static async Task<IActionResult> Run(
            [HttpTrigger(AuthorizationLevel.Anonymous, "get", Route = null)] HttpRequest req,
            ILogger log)
        {
            string response = string.Empty;
            string displayName = req.Query["displayname"];
            string deviceId = string.Empty;

            if (string.IsNullOrEmpty(displayName))
            {
                return new BadRequestObjectResult("failed, request missing device id");
            }

            deviceId = await GetDeviceIdAsync(displayName);

            if (string.IsNullOrEmpty(deviceId))
            {
                return new BadRequestObjectResult("failed, display name not found");
            }

            using (var client = new HttpClient())
            {
                client.BaseAddress = new Uri(iotCentralUrl);
                client.DefaultRequestHeaders.Add("Authorization", authorization);

                var api = $"api/preview/devices/{deviceId}/properties";

                try
                {
                    response = await client.GetStringAsync(api);
                    response = await UpdateIotCentral(response, client, new Uri(iotCentralUrl + "/" + api), deviceId);
                }
                catch
                {
                    return new BadRequestObjectResult("{}");
                }
            }

            return new OkObjectResult(response);
        }

        static async Task<string> GetDeviceIdAsync(string key)
        {
            string device_id = string.Empty;
            string response = string.Empty;

            using (var client = new HttpClient())
            {
                client.BaseAddress = new Uri(iotCentralUrl);
                client.DefaultRequestHeaders.Add("Authorization", authorization);

                var api = $"api/devices?api-version=1.0";

                try
                {
                    response = await client.GetStringAsync(api);

                    JObject jsondata = (JObject)JsonConvert.DeserializeObject(response);
                    var values = jsondata.GetValue("value");

                    foreach (var item in values)
                    {
                        var dn = item["displayName"].ToString();
                        if (dn.Equals(key, StringComparison.OrdinalIgnoreCase))
                        {
                            device_id = item["id"].ToString();
                            break;
                        }
                    }
                }
                catch
                {
                    device_id = string.Empty;
                }
            }
            return device_id;
        }

        static async Task<string> UpdateIotCentral(string response, HttpClient client, Uri requestUri, string deviceId)
        {
            IoTCentralPatch iotCentralPatch = new IoTCentralPatch();
            ChannelIdResponse channelIdResponse = new ChannelIdResponse();

            Random _randomNumber = new Random();
            bool patchRequired = false;

            var channelIdDefinition = new { DesiredChannelId = "" };
            var iotcChannelId = JsonConvert.DeserializeAnonymousType(response, channelIdDefinition);

            if (iotcChannelId.DesiredChannelId is null || string.IsNullOrEmpty(iotcChannelId.DesiredChannelId))
            {
                iotCentralPatch.DesiredChannelId = _randomNumber.Next(999999);
                patchRequired = true;
            }
            else
            {
                iotCentralPatch.DesiredChannelId = Int32.Parse(iotcChannelId.DesiredChannelId);
            }

            channelIdResponse.DesiredChannelId = iotCentralPatch.DesiredChannelId;
            string responseData = JsonConvert.SerializeObject(channelIdResponse, Formatting.None);

            iotCentralPatch.DeviceId = deviceId;
            string jsonData = JsonConvert.SerializeObject(iotCentralPatch, Formatting.None);

            if (patchRequired)
            {
                HttpContent httpContent = new StringContent(jsonData, Encoding.UTF8, "application/json");
                var result = await HttpClientExtensions.PatchAsync(client, requestUri, httpContent, authorization);
                if (!result.IsSuccessStatusCode)
                {
                    jsonData = "{}";
                    responseData = "{}";
                }
            }

            return responseData;
        }
    }
}
