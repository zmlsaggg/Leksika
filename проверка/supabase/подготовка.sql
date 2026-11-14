create role anon nologin;
create role authenticated nologin;
create schema auth;
create table auth.users (id uuid primary key, raw_user_meta_data jsonb);
create function auth.uid() returns uuid language sql stable as $$ select nullif(current_setting('request.uid', true), '')::uuid $$;
create function auth.jwt() returns jsonb language sql stable as $$ select coalesce(nullif(current_setting('request.jwt', true), '')::jsonb, '{}'::jsonb) $$;
grant usage on schema auth to anon, authenticated;
grant execute on function auth.uid(), auth.jwt() to anon, authenticated;
insert into auth.users(id, raw_user_meta_data)
select ('00000000-0000-4000-8000-' || lpad(i::text,12,'0'))::uuid, jsonb_build_object('user_name','user' || i)
from generate_series(1,6) i;
