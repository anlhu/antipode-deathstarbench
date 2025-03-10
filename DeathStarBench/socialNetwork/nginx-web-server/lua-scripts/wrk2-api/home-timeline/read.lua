local _M = {}
local xtracer = require "luaxtrace"

local function _StrIsEmpty(s)
  return s == nil or s == ''
end

local function _LoadTimeline(data)
  local timeline = {}
  for _, timeline_post in ipairs(data) do
    local new_post = {}
    new_post["post_id"] = tostring(timeline_post.post_id)
    new_post["creator"] = {}
    new_post["creator"]["user_id"] = tostring(timeline_post.creator.user_id)
    new_post["creator"]["username"] = timeline_post.creator.username
    new_post["req_id"] = tostring(timeline_post.req_id)
    new_post["text"] = timeline_post.text
    new_post["user_mentions"] = {}
    for _, user_mention in ipairs(timeline_post.user_mentions) do
      local new_user_mention = {}
      new_user_mention["user_id"] = tostring(user_mention.user_id)
      new_user_mention["username"] = user_mention.username
      table.insert(new_post["user_mentions"], new_user_mention)
    end
    new_post["media"] = {}
    for _, media in ipairs(timeline_post.media) do
      local new_media = {}
      new_media["media_id"] = tostring(media.media_id)
      new_media["media_type"] = media.media_type
      table.insert(new_post["media"], new_media)
    end
    new_post["urls"] = {}
    for _, url in ipairs(timeline_post.urls) do
      local new_url = {}
      new_url["shortened_url"] = url.shortened_url
      new_url["expanded_url"] = url.expanded_url
      table.insert(new_post["urls"], new_url)
    end
    new_post["timestamp"] = tostring(timeline_post.timestamp)
    new_post["post_type"] = timeline_post.post_type
    table.insert(timeline, new_post)
  end
  return timeline
end

function _M.ReadHomeTimeline()
  local bridge_tracer = require "opentracing_bridge_tracer"
  local ngx = ngx
  local GenericObjectPool = require "GenericObjectPool"
  local HomeTimelineServiceClient = require "social_network_HomeTimelineService"
  local cjson = require "cjson"
  local jwt = require "resty.jwt"
  local liblualongnumber = require "liblualongnumber"

  xtracer.StartLuaTrace("NginxWebServer", "ReadHomeTimeline")
  xtracer.LogXTrace("Processing Request")
  local req_id = tonumber(string.sub(ngx.var.request_id, 0, 15), 16)
  local tracer = bridge_tracer.new_from_global()
  local parent_span_context = tracer:binary_extract(
      ngx.var.opentracing_binary_context)
  local span = tracer:start_span("ReadHomeTimeline",
      {["references"] = {{"child_of", parent_span_context}}})
  local carrier = {}
  tracer:text_map_inject(span:context(), carrier)

  ngx.req.read_body()
  local args = ngx.req.get_uri_args()

  if (_StrIsEmpty(args.user_id) or _StrIsEmpty(args.start) or _StrIsEmpty(args.stop)) then
    ngx.status = ngx.HTTP_BAD_REQUEST
    ngx.say("Incomplete arguments")
    ngx.log(ngx.ERR, "Incomplete arguments")
    xtracer.LogXTrace("Incomplete arguments")
    xtracer.DeleteBaggage()
    ngx.exit(ngx.HTTP_BAD_REQUEST)
  end


  local client = GenericObjectPool:connection(
      HomeTimelineServiceClient, "home-timeline-service", 9090)
  carrier["baggage"] = xtracer.BranchBaggage()
  local status, ret = pcall(client.ReadHomeTimeline, client, req_id,
      tonumber(args.user_id), tonumber(args.start), tonumber(args.stop), carrier)
  GenericObjectPool:returnConnection(client)

  span:finish()
  if not status then
    ngx.status = ngx.HTTP_INTERNAL_SERVER_ERROR
    if (ret.message) then
      ngx.say("Get home-timeline failure: " .. ret.message)
      ngx.log(ngx.ERR, "Get home-timeline failure: " .. ret.message)
      xtracer.LogXTrace("Get home-timeline failure" .. ret.message)
    else
      ngx.say("Get home-timeline failure: " .. ret.message)
      ngx.log(ngx.ERR, "Get home-timeline failure: " .. ret.message)
      xtracer.LogXTrace("Get home-timeline failure" .. ret.message)
    end
    xtracer.DeleteBaggage()
    ngx.exit(ngx.HTTP_INTERNAL_SERVER_ERROR)
  else
    xtracer.JoinBaggage(ret.baggage)
    xtracer.LogXTrace("Loading timeline")
    local home_timeline = _LoadTimeline(ret.result)
    ngx.header.content_type = "application/json; charset=utf-8"
    ngx.say(cjson.encode(home_timeline) )

  end
  xtracer.DeleteBaggage()
end

return _M
